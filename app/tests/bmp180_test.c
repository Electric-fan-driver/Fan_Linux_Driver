#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "fan_api.h"

int main(void)
{
    struct timespec next;
    int temperature;
    int ret;

    ret = bmp180_open();
    if (ret) {
        fprintf(stderr, "BMP180 discovery failed: %s\n", strerror(-ret));
        return 1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &next) < 0) {
        perror("clock_gettime");
        bmp180_close();
        return 1;
    }

    puts("BMP180: reading every second. Stop with Ctrl+C.");

    for (;;) {
        ret = bmp180_get_temperature(&temperature);
        if (ret) {
            fprintf(stderr, "BMP180 read failed: %s\n", strerror(-ret));
            break;
        }

        printf("Temperature: %.1f C\n", temperature / 10.0);
        fflush(stdout);

        next.tv_sec++;
        do {
            ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                                  &next, NULL);
        } while (ret == EINTR);

        if (ret) {
            fprintf(stderr, "Timer failed: %s\n", strerror(ret));
            break;
        }
    }

    bmp180_close();
    return 1;
}
