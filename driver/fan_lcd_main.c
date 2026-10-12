#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "fan_lcd.h"

static int fan_lcd_probe(struct i2c_client *client)
{
    struct fan_lcd_data *data;
    char second_line[LCD_COLUMNS + 1];
    int ret;

    dev_info(&client->dev,
             "LCD1602 probe called: bus=%d, address=0x%02x\n",
             client->adapter->nr, client->addr);
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_WRITE_BYTE))
        return -EOPNOTSUPP;

    data = kzalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    data->client = client;
    data->backlight = true;
    mutex_init(&data->lock);
    kref_init(&data->ref);

    snprintf(second_line, sizeof(second_line), "I2C-%d ADDR 0x%02x",
             client->adapter->nr, client->addr);

    mutex_lock(&data->lock);
    ret = fan_lcd_initialize(data);
    if (!ret)
        ret = fan_lcd_write_line(data, 0x80, "LCD PROBE OK");
    if (!ret)
        ret = fan_lcd_write_line(data, 0xc0, second_line);
    mutex_unlock(&data->lock);

    if (ret) {
        dev_err(&client->dev, "LCD initialization/test output failed: %d\n", ret);
        goto free_data;
    }

    data->online = true;
    ret = fan_lcd_device_register(data);
    if (ret) {
        dev_err(&client->dev, "LCD misc registration failed: %d\n", ret);
        goto free_data;
    }

    i2c_set_clientdata(client, data);
    dev_info(&client->dev, "LCD initialized, /dev/lcd1602 registered\n");
    return 0;

free_data:
    mutex_lock(&data->lock);
    data->online = false;
    fan_lcd_set_backlight(data, false);
    mutex_unlock(&data->lock);
    kref_put(&data->ref, fan_lcd_free);
    return ret;
}

/* 열린 파일은 kref로 상태를 유지하며 이후 요청은 ENODEV로 실패한다. */
static int fan_lcd_remove(struct i2c_client *client)
{
    struct fan_lcd_data *data = i2c_get_clientdata(client);
    int ret;

    fan_lcd_device_unregister(data);
    mutex_lock(&data->lock);
    data->online = false;
    ret = fan_lcd_set_backlight(data, false);
    data->client = NULL;
    mutex_unlock(&data->lock);
    if (ret)
        dev_warn(&client->dev, "Backlight shutdown failed: %d\n", ret);
    i2c_set_clientdata(client, NULL);
    kref_put(&data->ref, fan_lcd_free);
    return 0;
}

static const struct of_device_id fan_lcd_of_match[] = {
    { .compatible = "kcci,lcd1602" },
    { }
};
MODULE_DEVICE_TABLE(of, fan_lcd_of_match);

static struct i2c_driver fan_lcd_driver = {
    .driver = {
        .name = "fan_lcd_driver",
        .of_match_table = fan_lcd_of_match,
    },
    .probe_new = fan_lcd_probe,
    .remove = fan_lcd_remove,
};

module_i2c_driver(fan_lcd_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fan LCD1602 I2C display and misc device driver");
