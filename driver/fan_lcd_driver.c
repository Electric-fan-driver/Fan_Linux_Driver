#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>

/* DT 매칭과 probe 호출만 확인한다. */
static int fan_lcd_probe(struct i2c_client *client)
{
    dev_info(&client->dev,
             "LCD1602 probe called: bus=%d, address=0x%02x\n",
             client->adapter->nr, client->addr);
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
};

module_i2c_driver(fan_lcd_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Fan LCD1602 DT matching and probe test");
