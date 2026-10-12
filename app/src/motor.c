#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "fan_api.h"

#define MOTOR_DEV	"/dev/fan_motor"
#define SERVO_DEV	"/dev/fan_servo"

static int motor_fd = -1; //-1 = 아직 안 열림
static int servo_fd = -1;

//장치 파일 열기
static int dev_open(const char *path, int *fd)
{
	if (*fd >= 0)
		return 0;
	*fd = open(path, O_RDWR | O_CLOEXEC);
	return *fd < 0 ? -errno : 0;
}

static void dev_close(int *fd)
{
	if (*fd >= 0) {
		close(*fd);
		*fd = -1;
	}
}

//모터
int motor_open(void)
{
	return dev_open(MOTOR_DEV, &motor_fd);
}

void motor_close(void)
{
	dev_close(&motor_fd);
}

int motor_set_level(int level)
{
	return ioctl(motor_fd, FAN_MOTOR_SET_LEVEL, &level) < 0 ? -errno : 0;	//0~4
}

//서보
int servo_open(void)
{
	return dev_open(SERVO_DEV, &servo_fd);
}

void servo_close(void)
{
	dev_close(&servo_fd);
}

int servo_set_swing(int enable)
{
	int val = enable ? 1 : 0;

	return ioctl(servo_fd, FAN_SERVO_SWING, &val) < 0 ? -errno : 0;
}
