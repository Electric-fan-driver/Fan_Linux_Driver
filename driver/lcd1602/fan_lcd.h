#ifndef FAN_LCD_H
#define FAN_LCD_H

#include <linux/i2c.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>

#define LCD_COLUMNS 16
#define FAN_LCD_SIZE 32

struct fan_lcd_data {
    struct i2c_client *client;
    struct mutex lock;
    struct kref ref;
    struct miscdevice misc;
    bool backlight;
    bool online;
};

/* HW 함수는 잠들 수 있으며 호출자가 lock을 잡아야 한다. */
int fan_lcd_initialize(struct fan_lcd_data *data);
int fan_lcd_write_line(struct fan_lcd_data *data, u8 address, const char *text);
int fan_lcd_set_cursor(struct fan_lcd_data *data, unsigned int offset);
int fan_lcd_put_char(struct fan_lcd_data *data, u8 character);
int fan_lcd_clear(struct fan_lcd_data *data);
int fan_lcd_set_backlight(struct fan_lcd_data *data, bool enable);

void fan_lcd_free(struct kref *ref);
int fan_lcd_device_register(struct fan_lcd_data *data);
void fan_lcd_device_unregister(struct fan_lcd_data *data);

#endif
