#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/compat.h>

#include "fan_lcd.h"
#include "fan_ioctl.h"

void fan_lcd_free(struct kref *ref)
{
    struct fan_lcd_data *data = container_of(ref, struct fan_lcd_data, ref);

    kfree(data);
}

static int fan_lcd_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct fan_lcd_data *data = container_of(misc, struct fan_lcd_data, misc);
    int ret = 0;

    mutex_lock(&data->lock);
    if (!data->online)
        ret = -ENODEV;
    else {
        kref_get(&data->ref);
        file->private_data = data;
    }
    mutex_unlock(&data->lock);
    return ret;
}

static int fan_lcd_release(struct inode *inode, struct file *file)
{
    struct fan_lcd_data *data = file->private_data;

    kref_put(&data->ref, fan_lcd_free);
    return 0;
}

/* offset 0..15: 첫째 행, 16..31: 둘째 행. 끝을 넘으면 짧은 쓰기. */
static ssize_t fan_lcd_write(struct file *file, const char __user *buffer,
                             size_t count, loff_t *position)
{
    struct fan_lcd_data *data = file->private_data;
    char text[FAN_LCD_SIZE];
    size_t done = 0;
    int ret;

    if (*position < 0 || *position > FAN_LCD_SIZE)
        return -EINVAL;
    if (!count)
        return 0;
    if (*position == FAN_LCD_SIZE)
        return -ENOSPC;

    count = min_t(size_t, count, FAN_LCD_SIZE - *position);
    if (copy_from_user(text, buffer, count))
        return -EFAULT;

    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;
    if (!data->online) {
        ret = -ENODEV;
        goto out;
    }

    ret = fan_lcd_set_cursor(data, *position);
    if (ret)
        goto out;

    for (done = 0; done < count; done++) {
        if (done && (*position + done) % LCD_COLUMNS == 0) {
            ret = fan_lcd_set_cursor(data, *position + done);
            if (ret)
                break;
        }
        ret = fan_lcd_put_char(data, text[done]);
        if (ret)
            break;
    }

out:
    *position += done;
    mutex_unlock(&data->lock);
    return done ? (ssize_t)done : ret;
}

static loff_t fan_lcd_llseek(struct file *file, loff_t offset, int whence)
{
    return fixed_size_llseek(file, offset, whence, FAN_LCD_SIZE);
}

static long fan_lcd_ioctl(struct file *file, unsigned int command,
                           unsigned long argument)
{
    struct fan_lcd_data *data = file->private_data;
    int enable = 0;
    int ret;

    switch (command) {
    case LCD_CLEAR:
        break;
    case LCD_BACKLIGHT:
        if (copy_from_user(&enable, (int __user *)argument, sizeof(enable)))
            return -EFAULT;
        if (enable != 0 && enable != 1)
            return -EINVAL;
        break;
    default:
        return -ENOTTY;
    }

    if (mutex_lock_interruptible(&data->lock))
        return -ERESTARTSYS;
    if (!data->online)
        ret = -ENODEV;
    else if (command == LCD_CLEAR)
        ret = fan_lcd_clear(data);
    else
        ret = fan_lcd_set_backlight(data, enable);
    mutex_unlock(&data->lock);
    return ret;
}

static const struct file_operations fan_lcd_fops = {
    .owner = THIS_MODULE,
    .open = fan_lcd_open,
    .release = fan_lcd_release,
    .write = fan_lcd_write,
    .llseek = fan_lcd_llseek,
    .unlocked_ioctl = fan_lcd_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = compat_ptr_ioctl,
#endif
};

int fan_lcd_device_register(struct fan_lcd_data *data)
{
    data->misc.minor = MISC_DYNAMIC_MINOR;
    data->misc.name = "lcd1602";
    data->misc.fops = &fan_lcd_fops;
    data->misc.parent = &data->client->dev;
    return misc_register(&data->misc);
}

void fan_lcd_device_unregister(struct fan_lcd_data *data)
{
    misc_deregister(&data->misc);
}
