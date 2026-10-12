#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/atomic.h>

#include "fan_ioctl.h"

#define FAN_LEVEL_MAX	4

static struct gpio_descs *leds;		//led-gpios 전체. desc[0] = 1단계, desc[3] = 4단계
static DEFINE_MUTEX(led_lock);		//GPIO 를 동시에 바꾸지 못하게 보호
static atomic_t opened;			// 1 = /dev/fan_led가 이미 열려있음

//레벨 n -> 앞에서부터 n개 켜고 나머지는 끔
static int led_show_level(int level)
{
	unsigned int i;

	if (level < 0 || level > FAN_LEVEL_MAX)
		return -EINVAL;

	mutex_lock(&led_lock);
	for (i = 0; i < leds->ndescs; i++)
		gpiod_set_value_cansleep(leds->desc[i], i < (unsigned int)level);
	mutex_unlock(&led_lock);
	return 0;
}

static int fan_led_open(struct inode *inode, struct file *filp)
{
	if (atomic_cmpxchg(&opened, 0, 1))
		return -EBUSY;
	return 0;
}

static int fan_led_release(struct inode *inode, struct file *filp)
{
	led_show_level(0);	//닫으면 모두 끔
	atomic_set(&opened, 0);
	return 0;
}

static long fan_led_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int __user *uarg = (int __user *)arg;
	int level;

	switch (cmd) {
	case FAN_LED_SET_LEVEL:		//레벨 0~4
		if (copy_from_user(&level, uarg, sizeof(level)))
			return -EFAULT;
		return led_show_level(level);

	default:
		return -ENOTTY;		//모르는 명령
	}
}

static const struct file_operations fan_led_fops = {
	.owner		= THIS_MODULE,
	.open		= fan_led_open,
	.release	= fan_led_release,
	.unlocked_ioctl	= fan_led_ioctl,
};

static struct miscdevice led_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "fan_led",
	.fops	= &fan_led_fops,
};

static int fan_led_probe(struct platform_device *pdev)
{
	//led-gpios 를 받아 오는 순간 모두 Low (꺼짐)
	leds = devm_gpiod_get_array(&pdev->dev, "led", GPIOD_OUT_LOW);
	if (IS_ERR(leds))
		return PTR_ERR(leds);

	return misc_register(&led_misc);
}

static int fan_led_remove(struct platform_device *pdev)
{
	misc_deregister(&led_misc);
	return 0;
}

static const struct of_device_id fan_led_of_match[] = {
	{ .compatible = "kcci,fan-led" },
	{ }
};

static struct platform_driver fan_led_driver = {
	.probe	= fan_led_probe,
	.remove	= fan_led_remove,
	.driver	= {
		.name = "fan_led",
		.of_match_table	= fan_led_of_match,
	},
};

static int __init fan_led_init(void)
{
	return platform_driver_register(&fan_led_driver);
}

static void __exit fan_led_exit(void)
{
	platform_driver_unregister(&fan_led_driver);
}

module_init(fan_led_init);
module_exit(fan_led_exit);

MODULE_LICENSE("GPL");
