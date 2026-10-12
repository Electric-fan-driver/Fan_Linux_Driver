#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "fan_api.h"

static volatile sig_atomic_t stop;

static void handle_stop(int signal_number)
{
    (void)signal_number;
    stop = 1;
}

int main(void)
{
    struct sigaction action = { .sa_handler = handle_stop };
    struct timespec next;
    int ret;
    int status = 1;

    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0) {
        perror("sigaction");
        return 1;
    }

    ret = bmp180_open();
    if (ret) {
        fprintf(stderr, "BMP180 open failed: %s\n", strerror(-ret));
        return 1;
    }
    ret = lcd_open();
    if (ret)
        goto out;
    ret = lcd_set_backlight(1);
    if (ret)
        goto out;
    ret = lcd_clear();
    if (ret)
        goto out;
    if (clock_gettime(CLOCK_MONOTONIC, &next) < 0) {
        ret = -errno;
        goto out;
    }

    puts("BMP180 -> LCD: actual temperature every second. Stop with Ctrl+C.");
    while (!stop) {
        int temperature = 0;
        int measurement_ret = bmp180_get_temperature(&temperature);

        ret = lcd_show_temp(temperature, measurement_ret == 0);
        if (ret)
            goto out;
        if (measurement_ret)
            fprintf(stderr, "BMP180 read failed: %s\n", strerror(-measurement_ret));
        else
            printf("Temperature: %.1f C\n", temperature / 10.0);
        fflush(stdout);

        next.tv_sec++;
        do {
            ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        } while (ret == EINTR && !stop);
        if (ret && ret != EINTR) {
            ret = -ret;
            goto out;
        }
    }
    ret = 0;
    status = 0;
out:
    if (ret)
        fprintf(stderr, "Temperature demo failed: %s\n", strerror(-ret));
    lcd_close();
    bmp180_close();
    return status;
}
