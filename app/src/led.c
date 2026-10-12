#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "fan_api.h"

#define LED_DEV	"/dev/fan_led"

static int led_fd = -1; //-1 = 아직 안 열림

int led_open(void)
{
	if (led_fd >= 0)
		return 0;
	led_fd = open(LED_DEV, O_RDWR | O_CLOEXEC);
	return led_fd < 0 ? -errno : 0;
}

void led_close(void)
{
	if (led_fd >= 0) {
		close(led_fd);
		led_fd = -1;
	}
}

int led_set_level(int level)
{
	return ioctl(led_fd, FAN_LED_SET_LEVEL, &level) < 0 ? -errno : 0;	//0~4
}
