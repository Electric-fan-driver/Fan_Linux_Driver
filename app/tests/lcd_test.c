#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

/* Common PCF8574 backpack: P0=RS, P1=RW, P2=E, P3=backlight,
 * P4..P7=D4..D7. A differently wired backpack needs different masks. */
static int fd;
static void pause_us(long us)
{
    struct timespec delay = { .tv_sec = us / 1000000,
                             .tv_nsec = (us % 1000000) * 1000 };
    while (nanosleep(&delay, &delay) < 0) {
        if (errno != EINTR) { perror("nanosleep"); exit(1); }
    }
}
static void output(unsigned char value)
{
    ssize_t n;
    do { n = write(fd, &value, 1); } while (n < 0 && errno == EINTR);
    if (n != 1) {
        if (n < 0) perror("LCD I2C write");
        else fputs("LCD I2C short write\n", stderr);
        close(fd);
        exit(1);
    }
}
static void nibble(unsigned char value, int rs)
{
    unsigned char pins = (value & 0xf0) | 0x08 | (rs ? 1 : 0);
    output(pins);
    output(pins | 0x04);
    pause_us(1);
    output(pins);
    pause_us(50);
}
static void byte(unsigned char value, int rs)
{
    nibble(value, rs);
    nibble(value << 4, rs);
}
static void line(unsigned char address, const char *s)
{
    byte(address, 0);
    while (*s) byte((unsigned char)*s++, 1);
}
int main(int argc, char **argv)
{
    const char *path = argc == 2 ? argv[1] : "/dev/i2c-1";
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [/dev/i2c-N]\n", argv[0]);
        return 1;
    }
    fd = open(path, O_RDWR);
    if (fd < 0) { perror(path); return 1; }
    /* Never force access to an address owned by a kernel driver. */
    if (ioctl(fd, I2C_SLAVE, 0x27) < 0) {
        perror("Select LCD address 0x27"); close(fd); return 1;
    }
    pause_us(50000);
    nibble(0x30, 0); pause_us(5000);
    nibble(0x30, 0); pause_us(200);
    nibble(0x30, 0);
    nibble(0x20, 0);
    byte(0x28, 0); /* Four-bit interface, two lines. */
    byte(0x08, 0);
    byte(0x01, 0); pause_us(2000);
    byte(0x06, 0);
    byte(0x0c, 0);
    line(0x80, "LCD TEST OK");
    line(0xc0, "I2C-1 ADDR 0x27");
    close(fd);
    puts("I2C writes completed. Check both lines on the LCD.");
    return 0;
}
