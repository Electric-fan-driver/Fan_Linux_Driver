#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "fan_bmp180.h"

/* 열린 파일마다 하나의 측정 문자열을 유지하여 짧은 read도 지원한다. */
struct fan_bmp180_file {
    struct fan_bmp180_data *data;
    struct mutex lock;
    char text[16];
    size_t length;
    bool valid;
};

void fan_bmp180_free(struct kref *ref)
{
    struct fan_bmp180_data *data = container_of(ref, struct fan_bmp180_data, ref);

    kfree(data);
}

static int fan_bmp180_open(struct inode *inode, struct file *file)
{
    struct miscdevice *misc = file->private_data;
    struct fan_bmp180_data *data = container_of(misc, struct fan_bmp180_data, misc);
    struct fan_bmp180_file *context;
    int ret = 0;

    context = kzalloc(sizeof(*context), GFP_KERNEL);
    if (!context)
        return -ENOMEM;
    mutex_init(&context->lock);

    mutex_lock(&data->lock);
    if (!data->online)
        ret = -ENODEV;
    else {
        kref_get(&data->ref);
        context->data = data;
        file->private_data = context;
    }
    mutex_unlock(&data->lock);
    if (ret)
        kfree(context);
    return ret;
}

static int fan_bmp180_release(struct inode *inode, struct file *file)
{
    struct fan_bmp180_file *context = file->private_data;

    kref_put(&context->data->ref, fan_bmp180_free);
    kfree(context);
    return 0;
}

static ssize_t fan_bmp180_read(struct file *file, char __user *buffer,
                               size_t count, loff_t *position)
{
    struct fan_bmp180_file *context = file->private_data;
    s32 temperature_mc;
    ssize_t ret;

    if (!count)
        return 0;
    if (*position < 0)
        return -EINVAL;
    if (mutex_lock_interruptible(&context->lock))
        return -ERESTARTSYS;

    if (!context->valid) {
        ret = fan_bmp180_get_temperature(context->data, &temperature_mc);
        if (ret)
            goto out;
        /* m°C -> 0.1°C: 예를 들어 24600 -> "246\n". */
        context->length = scnprintf(context->text, sizeof(context->text),
                                    "%d\n", temperature_mc / 100);
        context->valid = true;
    }

    ret = simple_read_from_buffer(buffer, count, position,
                                   context->text, context->length);
out:
    mutex_unlock(&context->lock);
    return ret;
}

/* 다음 측정은 lseek(fd, 0, SEEK_SET) 후 read로 요청한다. */
static loff_t fan_bmp180_llseek(struct file *file, loff_t offset, int whence)
{
    struct fan_bmp180_file *context = file->private_data;

    if (whence != SEEK_SET || offset != 0)
        return -EINVAL;
    if (mutex_lock_interruptible(&context->lock))
        return -ERESTARTSYS;
    context->valid = false;
    file->f_pos = 0;
    mutex_unlock(&context->lock);
    return 0;
}

static const struct file_operations fan_bmp180_fops = {
    .owner = THIS_MODULE,
    .open = fan_bmp180_open,
    .release = fan_bmp180_release,
    .read = fan_bmp180_read,
    .llseek = fan_bmp180_llseek,
};

int fan_bmp180_device_register(struct fan_bmp180_data *data)
{
    data->misc.minor = MISC_DYNAMIC_MINOR;
    data->misc.name = "bmp180";
    data->misc.fops = &fan_bmp180_fops;
    return misc_register(&data->misc);
}

void fan_bmp180_device_unregister(struct fan_bmp180_data *data)
{
    misc_deregister(&data->misc);
}
