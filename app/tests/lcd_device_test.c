#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "fan_ioctl.h"

static int write_at(int fd, const char *text, size_t length, off_t offset)
{
    ssize_t n = pwrite(fd, text, length, offset);

    if (n < 0) {
        perror("LCD pwrite");
        return -1;
    }
    if ((size_t)n != length) {
        fprintf(stderr, "LCD short write: %zd/%zu\n", n, length);
        return -1;
    }
    return 0;
}

int main(void)
{
    int fd = open("/dev/lcd1602", O_RDWR);
    int enable = 1;
    int status = 1;

    if (fd < 0) {
        perror("/dev/lcd1602");
        return 1;
    }
    if (ioctl(fd, LCD_BACKLIGHT, &enable) < 0 || ioctl(fd, LCD_CLEAR) < 0) {
        perror("LCD ioctl");
        goto out;
    }

    /* 행 경계(15 -> 16)를 넘는 쓰기도 확인한다. */
    if (write_at(fd, "AB", 2, 15))
        goto out;
    errno = 0;
    if (pwrite(fd, "X", 1, 32) != -1 || errno != ENOSPC) {
        fputs("Expected ENOSPC at offset 32\n", stderr);
        goto out;
    }

    puts("Check A at row 1 column 16 and B at row 2 column 1 (2 seconds).");
    sleep(2);
    if (ioctl(fd, LCD_CLEAR) < 0) {
        perror("LCD_CLEAR");
        goto out;
    }
    if (write_at(fd, "AUTO    00:30:00", 16, 0) ||
        write_at(fd, "24.6 C", 6, 16) || write_at(fd, "ROT", 3, 29))
        goto out;

    enable = 0;
    if (ioctl(fd, LCD_BACKLIGHT, &enable) < 0) {
        perror("Backlight off");
        goto out;
    }
    puts("Backlight off for 1 second.");
    sleep(1);
    enable = 1;
    if (ioctl(fd, LCD_BACKLIGHT, &enable) < 0) {
        perror("Backlight on");
        goto out;
    }
    puts("PASS: device writes and ioctls completed. Check the LCD display.");
    status = 0;
out:
    close(fd);
    return status;
}
