/* SPDX-License-Identifier: GPL-2.0 */
#include <errno.h>
#include <stdint.h>
#include "timer.h"

/* User-space timer logic. Caller supplies CLOCK_MONOTONIC milliseconds.
 * Call from one owning thread, or serialize calls externally. No worker,
 * device access, sleeping, or output is performed by these functions. */

enum timer_mode { TIMER_IDLE, TIMER_SETTING, TIMER_RUNNING };
static enum timer_mode mode;
static int setting_sec;
static int64_t last_change_ms;
static int64_t deadline_ms;
#define TIMER_STEP_SEC 600
#define TIMER_MAX_SEC (12 * 60 * 60)
#define TIMER_CONFIRM_MS 10000

void timer_reset(void)
{
    mode = TIMER_IDLE;
    setting_sec = 0;
    last_change_ms = 0;
    deadline_ms = 0;
}

static int remaining_sec(int64_t now)
{
    int64_t left = deadline_ms - now;
    if (left > (int64_t)TIMER_MAX_SEC * 1000)
        left = (int64_t)TIMER_MAX_SEC * 1000;
    return left > 0 ? (int)((left + 999) / 1000) : 0;
}

static void confirm(int64_t now)
{
    mode = TIMER_RUNNING;
    deadline_ms = now > INT64_MAX - (int64_t)setting_sec * 1000 ? INT64_MAX :
                  now + (int64_t)setting_sec * 1000;
}

void timer_rotate(int dir, long now_ms)
{
    int value, previous;
    int64_t now = now_ms;
    if (now < 0 || (dir != 1 && dir != -1))
        return;
    previous = mode == TIMER_SETTING ? setting_sec :
               mode == TIMER_RUNNING ? remaining_sec(now) : 0;
    value = previous;
    if (value % TIMER_STEP_SEC) {
        value = dir > 0 ? (value / TIMER_STEP_SEC + 1) * TIMER_STEP_SEC :
                         (value / TIMER_STEP_SEC) * TIMER_STEP_SEC;
    } else {
        value += dir * TIMER_STEP_SEC;
    }
    if (value < 0)
        value = 0;
    if (value > TIMER_MAX_SEC)
        value = TIMER_MAX_SEC;
    if (value == previous)
        return;
    if (!value) {
        mode = TIMER_IDLE;
        setting_sec = 0;
        deadline_ms = 0;
        return;
    }
    if (mode != TIMER_SETTING || value != previous)
        last_change_ms = now;
    setting_sec = value;
    mode = TIMER_SETTING;
    deadline_ms = 0;
}

void timer_button(long now_ms)
{
    if (now_ms < 0)
        return;
    if (mode == TIMER_SETTING)
        confirm(now_ms);
    else if (mode == TIMER_RUNNING) {
        mode = TIMER_IDLE;
        setting_sec = 0;
        deadline_ms = 0;
    }
}

int timer_tick(long now_ms)
{
    if (now_ms < 0)
        return -EINVAL;
    if (mode == TIMER_SETTING && (int64_t)now_ms - last_change_ms >= TIMER_CONFIRM_MS)
        confirm(last_change_ms + TIMER_CONFIRM_MS);
    if (mode == TIMER_RUNNING && (int64_t)now_ms >= deadline_ms) {
        timer_reset();
        return 1;
    }
    return 0;
}

void timer_get_display(int *sec, int *visible, long now_ms)
{
    int value = mode == TIMER_SETTING ? setting_sec :
                mode == TIMER_RUNNING && now_ms >= 0 ? remaining_sec(now_ms) : 0;
    if (sec)
        *sec = value;
    if (visible)
        *visible = mode != TIMER_IDLE && value > 0;
}
