#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "fan_api.h"

static int lcd_fd = -1;

/* 영역 전체를 공백까지 덮어써 이전 문자가 남지 않도록 한다. */
static int lcd_write_at(const char *text, size_t length, off_t offset)
{
    size_t done = 0;

    if (lcd_fd < 0)
        return -ENODEV;
    while (done < length) {
        ssize_t n = pwrite(lcd_fd, text + done, length - done, offset + done);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        if (!n)
            return -EIO;
        done += (size_t)n;
    }
    return 0;
}

int lcd_open(void)
{
    if (lcd_fd >= 0)
        return 0;
    lcd_fd = open("/dev/lcd1602", O_RDWR | O_CLOEXEC);
    return lcd_fd < 0 ? -errno : 0;
}

void lcd_close(void)
{
    if (lcd_fd >= 0) {
        close(lcd_fd);
        lcd_fd = -1;
    }
}

int lcd_clear(void)
{
    if (lcd_fd < 0)
        return -ENODEV;
    return ioctl(lcd_fd, LCD_CLEAR) < 0 ? -errno : 0;
}

int lcd_set_backlight(int enable)
{
    if (enable != 0 && enable != 1)
        return -EINVAL;
    if (lcd_fd < 0)
        return -ENODEV;
    return ioctl(lcd_fd, LCD_BACKLIGHT, &enable) < 0 ? -errno : 0;
}

int lcd_show_mode(int auto_mode)
{
    return lcd_write_at(auto_mode ? "AUTO" : "MAN ", 4, 0);
}

int lcd_show_timer(int sec, int visible)
{
    char text[9];

    if (!visible)
        return lcd_write_at("        ", 8, 8);
    if (sec < 0 || sec > 12 * 60 * 60)
        return -EINVAL;
    snprintf(text, sizeof(text), "%02d:%02d:%02d",
             sec / 3600, sec / 60 % 60, sec % 60);
    return lcd_write_at(text, 8, 8);
}

int lcd_show_temp(int temp_x10, int valid)
{
    char text[32];
    char field[7];
    long long magnitude = temp_x10;
    int length;

    if (!valid)
        return lcd_write_at("--.-\xdf" "C ", 7, 16);
    if (magnitude < 0)
        magnitude = -magnitude;
    length = snprintf(text, sizeof(text), "%s%lld.%lld%cC",
                      temp_x10 < 0 ? "-" : "", magnitude / 10,
                      magnitude % 10, 0xdf);
    if (length < 0 || length > (int)sizeof(field))
        return -ERANGE;
    memset(field, ' ', sizeof(field));
    memcpy(field, text, (size_t)length);
    return lcd_write_at(field, sizeof(field), 16);
}

int lcd_show_rot(int on)
{
    return lcd_write_at(on ? "ROT" : "   ", 3, 29);
}
