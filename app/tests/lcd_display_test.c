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
    unsigned int tick = 0;
    static const int temperatures[] = { 246, 250, 199, -1, -400 };

    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) < 0 ||
        sigaction(SIGTERM, &action, NULL) < 0) {
        perror("sigaction");
        return 1;
    }
    ret = lcd_open();
    if (ret)
        goto out;
    ret = lcd_set_backlight(1);
    if (ret)
        goto out;
    /* 화면은 시작할 때만 지우고 이후에는 온도 7칸만 갱신한다. */
    ret = lcd_clear();
    if (ret)
        goto out;
    ret = lcd_show_mode(1);
    if (ret)
        goto out;
    ret = lcd_show_timer(1800, 1);
    if (ret)
        goto out;
    ret = lcd_show_rot(1);
    if (ret)
        goto out;
    if (clock_gettime(CLOCK_MONOTONIC, &next) < 0) {
        ret = -errno;
        goto out;
    }

    puts("LCD-only test: simulated temperatures every second. Stop with Ctrl+C.");
    while (!stop) {
        unsigned int sample = tick % 6;
        int valid = sample < 5;
        int temperature = valid ? temperatures[sample] : 0;

        ret = lcd_show_temp(temperature, valid);
        if (ret)
            goto out;
        if (valid)
            printf("Simulated temperature: %.1f C\n", temperature / 10.0);
        else
            puts("Simulated invalid temperature: --.- C");
        tick++;
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
        fprintf(stderr, "LCD display test failed: %s\n", strerror(-ret));
    lcd_close();
    return status;
}
