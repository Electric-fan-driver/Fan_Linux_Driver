#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/iio/iio.h>

#include "fan_bmp180.h"

static int fan_bmp180_probe(struct i2c_client *client)
{
    struct iio_dev *indio_dev;
    struct fan_bmp180_data *data;
    int ret;

    indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*data));
    if (!indio_dev)
        return -ENOMEM;

    data = iio_priv(indio_dev);

    ret = fan_bmp180_sensor_init(data, client);
    if (ret)
        return ret;

    ret = fan_bmp180_iio_register(&client->dev, indio_dev);
    if (ret) {
        dev_err(&client->dev,
                "IIO registration failed: %d\n", ret);
        return ret;
    }

    i2c_set_clientdata(client, indio_dev);

    dev_info(&client->dev,
             "Chip ID verified: 0x%02x, IIO device registered\n",
             data->chip_id);

    return 0;
}

static const struct of_device_id fan_bmp180_of_match[] = {
    { .compatible = "kcci,bmp180" },
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
MODULE_DESCRIPTION("Fan BMP180 IIO temperature and pressure driver");
