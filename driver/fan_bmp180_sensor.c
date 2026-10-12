#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/limits.h>
#include <linux/iopoll.h>

#include "fan_bmp180.h"

#define BMP180_CHIP_ID_REG   0xD0        // can be used to check whether communication is functioning
#define BMP180_CHIP_ID       0x55    
#define BMP180_CALIB_START   0xAA        // AC1 parameter MSB
#define BMP180_CALIB_BYTES   22          // 11 calibration words, 2 bytes each
#define BMP180_CTRL_MEAS_REG 0xF4        // Controls measurements
#define BMP180_OUT_MSB_REG   0xF6        // adc_out_msb<7:0>
#define BMP180_TEMP_CMD      0x2E        // write 0x2E to read uncompensated temperature value
#define BMP180_PRESS_CMD     0x34        // write 0x34 to read uncompensated pressure value
#define BMP180_SCO           BIT(5)      // Start of conversion
#define BMP180_POLL_US       500      
#define BMP180_TIMEOUT_MARGIN_US 5000

/* 호출자가 data->lock을 유지한다. 
* SCO = 0 -> The value of this bit stays “1” during conversion 
* and is reset to “0” after conversion is complete
 * 최대 변환 시간에 여유를 더해 센서가 멈춰도 무한 대기하지 않는다. */
static int fan_bmp180_wait_conversion(struct fan_bmp180_data *data,
                                      unsigned int conversion_us)
{
    int status;
    int ret;

    ret = read_poll_timeout(i2c_smbus_read_byte_data, status,
                            status < 0 || !(status & BMP180_SCO),
                            BMP180_POLL_US,
                            conversion_us + BMP180_TIMEOUT_MARGIN_US,
                            false, data->client, BMP180_CTRL_MEAS_REG);
    if (status < 0)
        return status;

    return ret;
}

/* 초기화 시 한 번 읽어 저장 */
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

    for (i = 0; i < ARRAY_SIZE(words); i++) {
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

    ret = fan_bmp180_wait_conversion(data, 4500);
    if (ret)
        return ret;

    for (i = 0; i < ARRAY_SIZE(buf); i++) {
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
    static const unsigned int conversion_us[] = { 4500, 7500, 13500, 25500 };
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

    ret = fan_bmp180_wait_conversion(data, conversion_us[oss]);
    if (ret)
        return ret;

    for (i = 0; i < ARRAY_SIZE(buf); i++) {
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

/* 기압 보정에서도 사용하는 온도 보정값 b5를 함께 전달. */
static int fan_bmp180_compensate_temperature(
    const struct fan_bmp180_calibration *c,
    u16 ut, s32 *temperature_mc, s64 *b5_out)
{
    s64 x1, x2, b5;
    s64 denominator;
    s64 temperature;

    /* 온도 보정 */
    x1 = (((s64)ut - c->ac6) * c->ac5) >> 15;

    denominator = x1 + c->md;
    if (!denominator)
        return -ERANGE;

    x2 = div64_s64((s64)c->mc * 2048, denominator);
    b5 = x1 + x2;

    /* 0.1°C 단위의 결과를 m°C로 변환 */
    temperature = ((b5 + 8) >> 4) * 100;
    if (temperature < S32_MIN || temperature > S32_MAX)
        return -ERANGE;

    *temperature_mc = (s32)temperature;
    *b5_out = b5;

    return 0;
}

static int fan_bmp180_compensate(
    const struct fan_bmp180_calibration *c,
    u16 ut, u32 up, u8 oss,
    s32 *temperature_mc, u32 *pressure_pa)
{
    s64 x1, x2, x3;
    s64 b3, b5, b6;
    s32 temperature;
    s64 pressure;
    u64 b4, b7;
    u64 pressure_base;
    int ret;

    if (oss > 3)
        return -EINVAL;

    ret = fan_bmp180_compensate_temperature(c, ut, &temperature, &b5);
    if (ret)
        return ret;

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

    *temperature_mc = temperature;
    *pressure_pa = (u32)pressure;

    return 0;
}

/* 온도만 측정·보정. 잠들 수 있는 문맥에서 호출해야 한다.
 * 실패 시 출력값은 변경하지 않는다. */
int fan_bmp180_get_temperature(struct fan_bmp180_data *data,
                                s32 *temperature_mc)
{
    u16 raw_temperature;
    s32 temperature;
    s64 b5;
    int ret;

    mutex_lock(&data->lock);

    if (!data->online) {
        ret = -ENODEV;
        goto out;
    }

    ret = fan_bmp180_read_raw_temperature(data, &raw_temperature);
    if (ret)
        goto out;

    ret = fan_bmp180_compensate_temperature(&data->calib,
                                            raw_temperature,
                                            &temperature, &b5);
    if (!ret)
        *temperature_mc = temperature;

out:
    mutex_unlock(&data->lock);
    return ret;
}

/*
 * 온도·기압을 새로 측정하여 보정한 결과 전달.
 *
 * 반환값:
 *   0    : 성공, result 사용 가능
 *   음수 : 실패, result 사용 금지
 *
 * 잠들 수 있는 문맥에서 호출해야 한다.
 */
int fan_bmp180_get_measurement(
    struct fan_bmp180_data *data,
    struct fan_bmp180_measurement *result)
{
    struct fan_bmp180_measurement sample = { 0 };
    u8 oss;
    int ret;

    mutex_lock(&data->lock);

    if (!data->online) {
        ret = -ENODEV;
        goto out;
    }

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

    *result = sample;

out:
    mutex_unlock(&data->lock);

    return ret;
}

/* 장치 확인과 센서 전용 상태 초기화 */
int fan_bmp180_sensor_init(struct fan_bmp180_data *data,
                           struct i2c_client *client)
{
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
        dev_err(&client->dev,
                "Chip ID read failed: %d\n", chip_id);
        return chip_id;
    }

    if (chip_id != BMP180_CHIP_ID) {
        dev_err(&client->dev,
                "Unexpected Chip ID: 0x%02x\n", chip_id);
        return -ENODEV;
    }

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

    return 0;
}
