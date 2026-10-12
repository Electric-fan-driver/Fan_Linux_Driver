#ifndef FAN_TIMER_H
#define FAN_TIMER_H

/* User-space API; now_ms is CLOCK_MONOTONIC time in milliseconds.
 * All calls must be made by one owner or protected by the caller's lock. */
void timer_reset(void);
void timer_rotate(int dir, long now_ms);
void timer_button(long now_ms);
int timer_tick(long now_ms);
void timer_get_display(int *sec, int *visible, long now_ms);

#endif
