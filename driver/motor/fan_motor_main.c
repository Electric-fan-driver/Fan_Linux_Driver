#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/pwm.h>
#include <linux/gpio/consumer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>

#include "fan_ioctl.h"

#define FAN_LEVEL_MAX		4
#define KICK_DUTY_PERCENT	100	//킥스타트 듀티
#define KICK_TIME_MS		200	//킥스타트 시간

static const unsigned int level_duty[FAN_LEVEL_MAX + 1] = { 0, 45, 60, 80, 100 }; //레벨별 듀티

static struct pwm_device *motor_pwm;		//ENA 에 연결된 PWM
static struct gpio_desc *motor_in1;			//방향 핀 IN1
static DEFINE_MUTEX(motor_lock);			//cur_level, kicking 을 동시에 바꾸지 못하게 보호
static int cur_level;						//0~4
static bool kicking;						//킥스타트 중이면 true
static atomic_t opened;						// 1 = /dev/fan_motor가 이미 열려있음

//듀티를 PWM 으로 출력
static int motor_apply_duty(unsigned int percent)
{
	struct pwm_state st;

	pwm_init_state(motor_pwm, &st);
	pwm_set_relative_duty_cycle(&st, percent, 100);	//주기의 퍼센트만큼 High
	st.enabled = true;
	return pwm_apply_state(motor_pwm, &st);
}

//원래 레벨의 듀티로 낮추기
static void motor_kick_end(struct work_struct *work)
{
	mutex_lock(&motor_lock);
	if (kicking) { //0 으로 꺼졌으면 할 일 없음
		kicking = false;
		motor_apply_duty(level_duty[cur_level]);
	}
	mutex_unlock(&motor_lock);
}

static DECLARE_DELAYED_WORK(kick_work, motor_kick_end);	//킥스타트를 끝내는 예약 작업

//레벨 설정
static int motor_set_level(int level)
{
	int ret = 0;

	if (level < 0 || level > FAN_LEVEL_MAX)
		return -EINVAL;

	mutex_lock(&motor_lock);
	if (level == 0) {
		kicking = false; //예약 작업이 와도 아무것도 안 하게
		ret = motor_apply_duty(0);
		gpiod_set_value_cansleep(motor_in1, 0); //IN1 L -> 정지
	} else if (cur_level == 0) {
		gpiod_set_value_cansleep(motor_in1, 1); //IN1 H -> 정방향
		ret = motor_apply_duty(KICK_DUTY_PERCENT);
		if (!ret) {
			kicking = true;
			mod_delayed_work(system_wq, &kick_work, msecs_to_jiffies(KICK_TIME_MS)); //200 ms 뒤 motor_kick_end
		}
	} else if (!kicking) {
		ret = motor_apply_duty(level_duty[level]);
	}
	if (!ret)
		cur_level = level;
	mutex_unlock(&motor_lock);

	return ret;
}

static int fan_motor_open(struct inode *inode, struct file *filp)
{
	if (atomic_cmpxchg(&opened, 0, 1))
		return -EBUSY;
	return 0;
}

static int fan_motor_release(struct inode *inode, struct file *filp)
{
	motor_set_level(0);	//닫으면 모터 정지
	atomic_set(&opened, 0);
	return 0;
}

static long fan_motor_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int __user *uarg = (int __user *)arg;
	int level;

	switch (cmd) {
	case FAN_MOTOR_SET_LEVEL: //레벨 0~4
		if (copy_from_user(&level, uarg, sizeof(level)))
			return -EFAULT;
		return motor_set_level(level);

	case FAN_MOTOR_GET_LEVEL: //지금 레벨
		mutex_lock(&motor_lock);
		level = cur_level;
		mutex_unlock(&motor_lock);
		return copy_to_user(uarg, &level, sizeof(level)) ? -EFAULT : 0;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations fan_motor_fops = {
	.owner		= THIS_MODULE,
	.open		= fan_motor_open,
	.release	= fan_motor_release,
	.unlocked_ioctl	= fan_motor_ioctl,
};

static struct miscdevice motor_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= "fan_motor",
	.fops	= &fan_motor_fops,
};

static int fan_motor_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_desc *in2;
	int ret;

	motor_pwm = devm_pwm_get(dev, NULL);
	if (IS_ERR(motor_pwm))
		return PTR_ERR(motor_pwm);

	//IN1, IN2를 받아 오는 순간 둘 다 Low
	motor_in1 = devm_gpiod_get_index(dev, "dir", 0, GPIOD_OUT_LOW);
	if (IS_ERR(motor_in1))
		return PTR_ERR(motor_in1);
	in2 = devm_gpiod_get_index(dev, "dir", 1, GPIOD_OUT_LOW);	//계속 Low
	if (IS_ERR(in2))
		return PTR_ERR(in2);

	//처음 상태 -> 모터 정지
	ret = motor_apply_duty(0);
	if (ret)
		return ret;

	return misc_register(&motor_misc);
}

static int fan_motor_remove(struct platform_device *pdev)
{
	misc_deregister(&motor_misc);
	cancel_delayed_work_sync(&kick_work);	//예약된 킥스타트 작업 취소
	return 0;
}

static const struct of_device_id fan_motor_of_match[] = {
	{ .compatible = "kcci,fan-motor" },
	{ }
};

static struct platform_driver fan_motor_driver = {
	.probe	= fan_motor_probe,
	.remove	= fan_motor_remove,
	.driver	= {
		.name = "fan_motor",
		.of_match_table	= fan_motor_of_match,
	},
};

static int __init fan_motor_init(void)
{
	return platform_driver_register(&fan_motor_driver);
}

static void __exit fan_motor_exit(void)
{
	platform_driver_unregister(&fan_motor_driver);
}

module_init(fan_motor_init);
module_exit(fan_motor_exit);

MODULE_LICENSE("GPL");
