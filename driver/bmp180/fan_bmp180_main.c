#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "fan_bmp180.h"

static int fan_bmp180_probe(struct i2c_client *client)
{
    struct fan_bmp180_data *data;
    int ret;

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    kref_init(&data->ref);

    ret = fan_bmp180_sensor_init(data, client);
    if (ret)
        goto free_data;

    data->online = true;
    ret = fan_bmp180_device_register(data);
    if (ret) {
        dev_err(&client->dev,
                "Character device registration failed: %d\n", ret);
        goto free_data;
    }

    i2c_set_clientdata(client, data);

    dev_info(&client->dev,
             "Chip ID verified: 0x%02x, /dev/bmp180 registered\n",
             data->chip_id);

    return 0;

free_data:
    kref_put(&data->ref, fan_bmp180_free);
    return ret;
}

static int fan_bmp180_remove(struct i2c_client *client)
{
    struct fan_bmp180_data *data = i2c_get_clientdata(client);

    fan_bmp180_device_unregister(data);
    mutex_lock(&data->lock);
    data->online = false;
    data->client = NULL;
    mutex_unlock(&data->lock);
    i2c_set_clientdata(client, NULL);
    kref_put(&data->ref, fan_bmp180_free);
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
    .remove = fan_bmp180_remove,
};

module_i2c_driver(fan_bmp180_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fan BMP180 temperature character device driver");
