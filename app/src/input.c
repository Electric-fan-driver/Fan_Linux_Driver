#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "fan_api.h"

static int input_fd = -1;

int fan_input_open(void)
{
    if (input_fd >= 0)
        return 0;
    input_fd = open("/dev/fan_input", O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    return input_fd < 0 ? -errno : 0;
}

void fan_input_close(void)
{
    if (input_fd >= 0) {
        close(input_fd);
        input_fd = -1;
    }
}

int fan_input_fd(void)
{
    return input_fd >= 0 ? input_fd : -ENODEV;
}

int fan_input_get_event(struct fan_input_event *ev)
{
    struct fan_input_event event;
    ssize_t n;

    if (!ev)
        return -EINVAL;
    if (input_fd < 0)
        return -ENODEV;

    do {
        n = read(input_fd, &event, sizeof(event));
    } while (n < 0 && errno == EINTR);

    if (n < 0)
        return -errno;
    if (n != (ssize_t)sizeof(event))
        return -EIO;

    *ev = event;
    return 0;
}
