#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include "fan_api.h"

static int bmp180_fd = -1;

int bmp180_open(void)
{
    if (bmp180_fd >= 0)
        return 0;
    bmp180_fd = open("/dev/bmp180", O_RDONLY | O_CLOEXEC);
    return bmp180_fd < 0 ? -errno : 0;
}

void bmp180_close(void)
{
    if (bmp180_fd >= 0) {
        close(bmp180_fd);
        bmp180_fd = -1;
    }
}

int bmp180_get_temperature(int *temp_x10)
{
    char text[16];
    char *end;
    size_t length = 0;
    long temperature;

    if (!temp_x10)
        return -EINVAL;
    if (bmp180_fd < 0)
        return -ENODEV;
    if (lseek(bmp180_fd, 0, SEEK_SET) < 0)
        return -errno;

    while (length < sizeof(text) - 1) {
        ssize_t n = read(bmp180_fd, text + length, sizeof(text) - 1 - length);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -errno;
        }
        if (!n)
            break;
        length += (size_t)n;
    }
    if (!length)
        return -EIO;
    if (length == sizeof(text) - 1)
        return -EOVERFLOW;
    text[length] = '\0';
    errno = 0;
    temperature = strtol(text, &end, 10);
    if (errno == ERANGE || temperature < INT_MIN || temperature > INT_MAX)
        return -ERANGE;
    if (end == text || end != text + length - 1 || *end != '\n')
        return -EIO;

    *temp_x10 = (int)temperature;
    return 0;
}
