/* fan_api.h — 앱 모듈 공용 API. 반환: 0 성공, 음수 errno 실패. 관리자: D */
#ifndef _FAN_API_H
#define _FAN_API_H

#include "fan_ioctl.h"   /* struct fan_input_event */

/* A: input.c (D: input_sim.c — 같은 함수, SIM=1 로 선택) */
int  fan_input_open(void);
void fan_input_close(void);
int  fan_input_fd(void);                               /* epoll 등록용 */
int  fan_input_get_event(struct fan_input_event *ev);  /* 비대기, 없으면 -EAGAIN */

/* A: timer.c — Timer 하위 FSM (3.3장). 장치 없는 순수 로직, 시간은 CLOCK_MONOTONIC ms */
void timer_reset(void);                                /* IDLE, 00:00:00 */
void timer_rotate(int dir, long now_ms);               /* 엔코더 +1 / -1 */
void timer_button(long now_ms);                        /* SETTING: 확정, RUNNING: 취소 */
int  timer_tick(long now_ms);                          /* 주기 호출, 1 = 만료(IDLE로 돌아감) */
void timer_get_display(int *sec, int *visible, long now_ms);

/* A: led.c */
int  led_open(void);
void led_close(void);
int  led_set_level(int level);                         /* 0~4 */

/* B: bmp180.c */
int  bmp180_open(void);
void bmp180_close(void);
int  bmp180_get_temperature(int *temp_x10);           /* 246 = 24.6℃ */

/* B: display.c — 칸 위치는 B만 안다 */
int  lcd_open(void);
void lcd_close(void);
int  lcd_clear(void);
int  lcd_set_backlight(int enable);
int  lcd_show_mode(int auto_mode);                     /* "MAN " / "AUTO" */
int  lcd_show_timer(int sec, int visible);             /* visible=0 → 공백 8칸 */
int  lcd_show_temp(int temp_x10, int valid);           /* valid=0 → "--.-°C" */
int  lcd_show_rot(int on);

/* C: motor.c */
int  motor_open(void);  void motor_close(void);
int  motor_set_level(int level);                       /* 0~4 */
int  servo_open(void);  void servo_close(void);
int  servo_set_swing(int enable);

#endif /* _FAN_API_H */
