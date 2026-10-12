#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/property.h>
#include <linux/pwm.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/atomic.h>
#include <linux/math64.h>

#include "fan_ioctl.h"

#define CENTER_DEG	90 //처음 각도
#define STEP_MS		25 //회전할 때 25 ms 마다 1° 씩 움직임 (초당 약 40°)

static struct pwm_device *servo_pwm; 	//서보 신호선에 연결된 PWM
static DEFINE_MUTEX(servo_lock);
static u32 min_pulse_us;				//0도일 때 펄스 폭
static u32 max_pulse_us;				//180도일 때 펄스 폭
static int min_deg;						//회전 범위
static int max_deg;
static int cur_deg;
static int dir = +1;					//+1=증가, -1=감소
static bool swinging;					//회전 중이면 true
static atomic_t opened;

static void servo_swing_step(struct work_struct *work);
static DECLARE_DELAYED_WORK(swing_work, servo_swing_step);	//STEP_MS 마다 1도 움직이는 예약 작업

//각도를 펄스 폭으로 바꿔 PWM 으로 출력
static int servo_apply_pos(int deg)
{
	struct pwm_state st;
	u64 span_ns = (u64)(max_pulse_us - min_pulse_us) * 1000;

	pwm_init_state(servo_pwm, &st);		//5 ms로 채움
	st.duty_cycle = (u64)min_pulse_us * 1000 + div_u64(span_ns * deg, 180);
	st.enabled = true;
	return pwm_apply_state(servo_pwm, &st);
}

//STEP_MS 마다 실행. 1도 움직이고, 끝에 닿으면 방향을 바꿈
static void servo_swing_step(struct work_struct *work)
{
	mutex_lock(&servo_lock);
	if (swinging) { //회전을 껐으면 아무것도 안 함
		cur_deg += dir;
		if (cur_deg >= max_deg) {
			cur_deg = max_deg;
			dir = -1;
		} else if (cur_deg <= min_deg) {
			cur_deg = min_deg;
			dir = +1;
		}

		servo_apply_pos(cur_deg);
		schedule_delayed_work(&swing_work, msecs_to_jiffies(STEP_MS));
	}
	mutex_unlock(&servo_lock);
}

//회전 켜기(1) / 끄기(0)
static int servo_swing_ctl(int enable)
{
	if (enable != 0 && enable != 1)
		return -EINVAL;

	mutex_lock(&servo_lock);
	if (enable && !swinging) {
		swinging = true;
		schedule_delayed_work(&swing_work, msecs_to_jiffies(STEP_MS));
	} else if (!enable && swinging) {
		swinging = false;
	}
	mutex_unlock(&servo_lock);
	return 0;
}

static int fan_servo_open(struct inode *inode, struct file *filp)
{
	if (atomic_cmpxchg(&opened, 0, 1))
		return -EBUSY;
	return 0;
}

static int fan_servo_release(struct inode *inode, struct file *filp)
{
	servo_swing_ctl(0); //닫으면 회전 정지
	atomic_set(&opened, 0);
	return 0;
}

static long fan_servo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int __user *uarg = (int __user *)arg;
	int val;

	switch (cmd) {
	case FAN_SERVO_SWING: //1 시작, 0 정지
		if (copy_from_user(&val, uarg, sizeof(val)))
			return -EFAULT;
		return servo_swing_ctl(val); //0, 1 이 아니면 -EINVAL

	case FAN_SERVO_GET_ANGLE: //지금 각도
		mutex_lock(&servo_lock);
		val = cur_deg;
		mutex_unlock(&servo_lock);
		return copy_to_user(uarg, &val, sizeof(val)) ? -EFAULT : 0;

	default:
		return -ENOTTY;
	}
}

static const struct file_operations fan_servo_fops = {
	.owner = THIS_MODULE,
	.open = fan_servo_open,
	.release = fan_servo_release,
	.unlocked_ioctl	= fan_servo_ioctl,
};

// /dev/fan_servo
static struct miscdevice servo_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "fan_servo",
	.fops = &fan_servo_fops,
};

//DT에서 숫자 하나 읽기. 없으면 기본값
static u32 prop_u32(struct device *dev, const char *name, u32 def)
{
	u32 v;

	return device_property_read_u32(dev, name, &v) ? def : v;
}

static int fan_servo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	servo_pwm = devm_pwm_get(dev, NULL);
	if (IS_ERR(servo_pwm))
		return PTR_ERR(servo_pwm);

	min_pulse_us = prop_u32(dev, "min-pulse-us", 500);
	max_pulse_us = prop_u32(dev, "max-pulse-us", 2500);
	min_deg = prop_u32(dev, "swing-min-deg", 30);
	max_deg = prop_u32(dev, "swing-max-deg", 150);

	//처음 상태: 90도, 회전 꺼짐
	cur_deg = CENTER_DEG;
	ret = servo_apply_pos(cur_deg);
	if (ret)
		return ret;

	ret = misc_register(&servo_misc);
	if (ret) {
		pwm_disable(servo_pwm);
		return ret;
	}
	return 0;
}

static int fan_servo_remove(struct platform_device *pdev)
{
	misc_deregister(&servo_misc);
	cancel_delayed_work_sync(&swing_work); //남은 회전 예약을 취소하고 끝날 때까지 기다림
	pwm_disable(servo_pwm); //펄스 끊기
	return 0;
}

static const struct of_device_id fan_servo_of_match[] = {
	{ .compatible = "kcci,fan-servo" },
	{ }
};

static struct platform_driver fan_servo_driver = {
	.probe	= fan_servo_probe,
	.remove	= fan_servo_remove,
	.driver	= {
		.name = "fan_servo",
		.of_match_table	= fan_servo_of_match,
	},
};

static int __init fan_servo_init(void)
{
	return platform_driver_register(&fan_servo_driver);
}

static void __exit fan_servo_exit(void)
{
	platform_driver_unregister(&fan_servo_driver);
}

module_init(fan_servo_init);
module_exit(fan_servo_exit);

MODULE_LICENSE("GPL");
