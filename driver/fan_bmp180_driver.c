#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/limits.h>

#define BMP180_CHIP_ID_REG   0xD0
#define BMP180_CHIP_ID       0x55
#define BMP180_CALIB_START   0xAA
#define BMP180_CALIB_BYTES   22
#define BMP180_CTRL_MEAS_REG 0xF4
#define BMP180_OUT_MSB_REG   0xF6
#define BMP180_TEMP_CMD      0x2E
#define BMP180_PRESS_CMD     0x34

struct fan_bmp180_calibration {
    s16 ac1;
    s16 ac2;
    s16 ac3;
    u16 ac4;
    u16 ac5;
    u16 ac6;
    s16 b1;
    s16 b2;
    s16 mb;
    s16 mc;
    s16 md;
};

struct fan_bmp180_data {
    struct i2c_client *client;
    struct fan_bmp180_calibration calib;
    struct mutex lock;
    u8 chip_id;
    u8 oss;
};

struct fan_bmp180_measurement {
    u16 raw_temperature;
    u32 raw_pressure;
    s32 temperature_mc;
    u32 pressure_pa;
};

/* 장치 초기화 시 한 번 읽어 저장 */
static int fan_bmp180_read_calibration(struct fan_bmp180_data *data)
{
    struct fan_bmp180_calibration *c = &data->calib;
    u8 buf[BMP180_CALIB_BYTES];
    u16 words[11];
    int value;
    int i;

    for (i = 0; i < BMP180_CALIB_BYTES; i++) {
        value = i2c_smbus_read_byte_data(data->client,
                                       BMP180_CALIB_START + i);
        if (value < 0)
            return value;

        buf[i] = (u8)value;
    }

    for (i = 0; i < 11; i++) {
        words[i] = ((u16)buf[2 * i] << 8) | buf[2 * i + 1];

        if (words[i] == 0 || words[i] == 0xFFFF) {
            dev_err(&data->client->dev,
                    "Invalid calibration word %d: 0x%04x\n",
                    i, words[i]);
            return -EINVAL;
        }
    }

    c->ac1 = (s16)words[0];
    c->ac2 = (s16)words[1];
    c->ac3 = (s16)words[2];
    c->ac4 = words[3];
    c->ac5 = words[4];
    c->ac6 = words[5];
    c->b1  = (s16)words[6];
    c->b2  = (s16)words[7];
    c->mb  = (s16)words[8];
    c->mc  = (s16)words[9];
    c->md  = (s16)words[10];

    dev_info(&data->client->dev,
             "Calibration: AC1=%d AC2=%d AC3=%d AC4=%u AC5=%u AC6=%u\n",
             c->ac1, c->ac2, c->ac3,
             (unsigned int)c->ac4,
             (unsigned int)c->ac5,
             (unsigned int)c->ac6);

    dev_info(&data->client->dev,
             "Calibration: B1=%d B2=%d MB=%d MC=%d MD=%d\n",
             c->b1, c->b2, c->mb, c->mc, c->md);

    return 0;
}

/* 호출자가 data->lock을 잡은 상태에서 사용 */
static int fan_bmp180_read_raw_temperature(struct fan_bmp180_data *data,
                                          u16 *ut)
{
    u8 buf[2];
    int ret;
    int i;

    ret = i2c_smbus_write_byte_data(data->client,
                                   BMP180_CTRL_MEAS_REG,
                                   BMP180_TEMP_CMD);
    if (ret < 0)
        return ret;

    msleep(5);

    for (i = 0; i < 2; i++) {
        ret = i2c_smbus_read_byte_data(data->client,
                                     BMP180_OUT_MSB_REG + i);
        if (ret < 0)
            return ret;

        buf[i] = (u8)ret;
    }

    *ut = ((u16)buf[0] << 8) | buf[1];
    return 0;
}

/* 호출자가 data->lock을 잡은 상태에서 사용 */
static int fan_bmp180_read_raw_pressure(struct fan_bmp180_data *data,
                                       u8 oss, u32 *up)
{
    static const unsigned int wait_ms[] = { 5, 8, 14, 26 };
    u8 buf[3];
    int ret;
    int i;

    if (oss > 3)
        return -EINVAL;

    ret = i2c_smbus_write_byte_data(data->client,
                                   BMP180_CTRL_MEAS_REG,
                                   BMP180_PRESS_CMD | (oss << 6));
    if (ret < 0)
        return ret;

    msleep(wait_ms[oss]);

    for (i = 0; i < 3; i++) {
        ret = i2c_smbus_read_byte_data(data->client,
                                     BMP180_OUT_MSB_REG + i);
        if (ret < 0)
            return ret;

        buf[i] = (u8)ret;
    }

    *up = (((u32)buf[0] << 16) |
           ((u32)buf[1] << 8) |
           buf[2]) >> (8 - oss);

    return 0;
}

static int fan_bmp180_compensate(
    const struct fan_bmp180_calibration *c,
    u16 ut, u32 up, u8 oss,
    s32 *temperature_mc, u32 *pressure_pa)
{
    s64 x1, x2, x3;
    s64 b3, b5, b6;
    s64 denominator;
    s64 temperature;
    s64 pressure;
    u64 b4, b7;
    u64 pressure_base;

    if (oss > 3)
        return -EINVAL;

    /* 온도 보정 */
    x1 = (((s64)ut - c->ac6) * c->ac5) >> 15;
    denominator = x1 + c->md;
    if (!denominator)
        return -ERANGE;

    x2 = div64_s64((s64)c->mc * 2048, denominator);
    b5 = x1 + x2;

    temperature = ((b5 + 8) >> 4) * 100;
    if (temperature < S32_MIN || temperature > S32_MAX)
        return -ERANGE;

    /* 기압 보정 */
    b6 = b5 - 4000;

    x1 = ((s64)c->b2 * ((b6 * b6) >> 12)) >> 11;
    x2 = ((s64)c->ac2 * b6) >> 11;
    x3 = x1 + x2;

    b3 = ((((s64)c->ac1 * 4 + x3) * (1LL << oss)) + 2) >> 2;

    x1 = ((s64)c->ac3 * b6) >> 13;
    x2 = ((s64)c->b1 * ((b6 * b6) >> 12)) >> 16;
    x3 = (x1 + x2 + 2) >> 2;

    if (x3 + 32768 <= 0)
        return -ERANGE;

    b4 = ((u64)c->ac4 * (u64)(x3 + 32768)) >> 15;
    if (!b4)
        return -ERANGE;

    if ((s64)up < b3)
        return -ERANGE;

    b7 = (u64)((s64)up - b3) * (50000U >> oss);

    if (b7 < 0x80000000ULL)
        pressure_base = div64_u64(b7 * 2, b4);
    else
        pressure_base = div64_u64(b7, b4) * 2;

    if (pressure_base > S32_MAX)
        return -ERANGE;

    pressure = (s64)pressure_base;

    x1 = (pressure >> 8) * (pressure >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * pressure) >> 16;

    pressure += (x1 + x2 + 3791) >> 4;
    if (pressure <= 0 || pressure > S32_MAX)
        return -ERANGE;

    *temperature_mc = (s32)temperature;
    *pressure_pa = (u32)pressure;

    return 0;
}

/*
 * 공통 측정 함수
 *
 * 호출할 때마다 새 온도와 기압을 측정하여 보정한다.
 * msleep과 mutex를 사용하므로 잠들 수 있는 문맥에서 호출해야 한다.
 *
 * 반환값:
 *   0    : 성공, result 사용 가능
 *   음수 : 실패, result 사용 금지
 *
 * 호출자는 자신의 result 구조체를 제공한다.
 */
static int fan_bmp180_get_measurement(
    struct fan_bmp180_data *data,
    struct fan_bmp180_measurement *result)
{
    struct fan_bmp180_measurement sample;
    int temperature_tenths;
    int temperature_abs;
    u8 oss;
    int ret;

    mutex_lock(&data->lock);

    oss = data->oss;

    ret = fan_bmp180_read_raw_temperature(data,
                                          &sample.raw_temperature);
    if (ret)
        goto out;

    ret = fan_bmp180_read_raw_pressure(data, oss,
                                       &sample.raw_pressure);
    if (ret)
        goto out;

    ret = fan_bmp180_compensate(&data->calib,
                                sample.raw_temperature,
                                sample.raw_pressure,
                                oss,
                                &sample.temperature_mc,
                                &sample.pressure_pa);
    if (ret)
        goto out;

    /* 모든 작업이 성공한 경우에만 호출자의 결과를 갱신 */
    *result = sample;

out:
    mutex_unlock(&data->lock);

    if (ret)
        return ret;

    /* 테스트용 로그: 사용자 인터페이스 연결 후 제거 가능 */
    temperature_tenths = DIV_ROUND_CLOSEST(sample.temperature_mc, 100);
    temperature_abs = temperature_tenths < 0
                    ? -temperature_tenths : temperature_tenths;

    dev_info(&data->client->dev,
             "Raw measurement: UT=%u, UP=%u, OSS=%u\n",
             (unsigned int)sample.raw_temperature,
             sample.raw_pressure,
             (unsigned int)oss);

    dev_info(&data->client->dev,
             "Compensated: temperature=%s%d.%d C, pressure=%u Pa\n",
             temperature_tenths < 0 ? "-" : "",
             temperature_abs / 10,
             temperature_abs % 10,
             sample.pressure_pa);

    return 0;
}

static int fan_bmp180_probe(struct i2c_client *client)
{
    struct fan_bmp180_data *data;
    struct fan_bmp180_measurement test_result;
    int chip_id;
    int ret;

    if (!i2c_check_functionality(client->adapter,
                                I2C_FUNC_SMBUS_BYTE_DATA)) {
        dev_err(&client->dev,
                "Byte-data read/write is not supported\n");
        return -EOPNOTSUPP;
    }

    chip_id = i2c_smbus_read_byte_data(client, BMP180_CHIP_ID_REG);
    if (chip_id < 0) {
        dev_err(&client->dev, "Chip ID read failed: %d\n", chip_id);
        return chip_id;
    }

    if (chip_id != BMP180_CHIP_ID) {
        dev_err(&client->dev,
                "Unexpected Chip ID: 0x%02x\n", chip_id);
        return -ENODEV;
    }

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->client = client;
    data->chip_id = (u8)chip_id;
    data->oss = 0;
    mutex_init(&data->lock);

    ret = fan_bmp180_read_calibration(data);
    if (ret) {
        dev_err(&client->dev,
                "Calibration initialization failed: %d\n", ret);
        return ret;
    }

    i2c_set_clientdata(client, data);

    /*
     * 임시 테스트 호출.
     */
    ret = fan_bmp180_get_measurement(data, &test_result);
    if (ret) {
        dev_err(&client->dev,
                "Test measurement failed: %d\n", ret);
        i2c_set_clientdata(client, NULL);
        return ret;
    }

    dev_info(&client->dev,
             "Chip ID verified: 0x%02x, device data initialized\n",
             data->chip_id);

    return 0;
}

static const struct of_device_id fan_bmp180_of_match[] = {
    { .compatible = "bosch,bmp180" },
    { }
};
MODULE_DEVICE_TABLE(of, fan_bmp180_of_match);

static struct i2c_driver fan_bmp180_driver = {
    .driver = {
        .name = "fan_bmp180_driver",
        .of_match_table = fan_bmp180_of_match,
    },
    .probe_new = fan_bmp180_probe,
};

module_i2c_driver(fan_bmp180_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fan BMP180 I2C sensor driver");