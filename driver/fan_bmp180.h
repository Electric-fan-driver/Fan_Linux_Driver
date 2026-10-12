#ifndef FAN_BMP180_H
#define FAN_BMP180_H

#include <linux/types.h>
#include <linux/mutex.h>

struct i2c_client;
struct iio_dev;
struct device;

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

int fan_bmp180_sensor_init(struct fan_bmp180_data *data,
                           struct i2c_client *client);
int fan_bmp180_get_measurement(struct fan_bmp180_data *data,
                               struct fan_bmp180_measurement *result);
int fan_bmp180_iio_register(struct device *dev, struct iio_dev *indio_dev);

#endif /* FAN_BMP180_H */
