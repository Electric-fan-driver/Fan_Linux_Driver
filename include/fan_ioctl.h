/* fan_ioctl.h — 드라이버와 fanctl이 공유. 관리자: D */
#ifndef _FAN_IOCTL_H
#define _FAN_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>   /* __u8 등: 커널·유저 공간 공용 */

#define FAN_IOCTL_VERSION 1          /* 인터페이스 변경 시 증가 */
#define FAN_IOC_MAGIC     'F'

/* 입력 이벤트 (/dev/fan_input read) */
enum { FAN_EV_BTN = 0, FAN_EV_ROT = 1 };                  /* type */
enum { BTN_POWER = 0, BTN_SWING = 1,
       BTN_ENC_FAN = 2, BTN_ENC_TIMER = 3 };              /* type == FAN_EV_BTN 일 때 id */
enum { ENC_FAN = 0, ENC_TIMER = 1 };                      /* type == FAN_EV_ROT 일 때 id */

struct fan_input_event {      /* 8 바이트 고정 */
    __u8  type;
    __u8  id;
    __s8  value;              /* BTN: 1 / ROT: +1(CW), -1(CCW) */
    __u8  _pad;
    __u32 ts_ms;
};

/* ioctl 명령 (인자는 int 포인터) */
#define FAN_LED_SET_LEVEL    _IOW(FAN_IOC_MAGIC, 1, int)   /* 0~4 */
#define FAN_MOTOR_SET_LEVEL  _IOW(FAN_IOC_MAGIC, 2, int)   /* 0~4 */
#define FAN_MOTOR_GET_LEVEL  _IOR(FAN_IOC_MAGIC, 3, int)
#define FAN_SERVO_SWING      _IOW(FAN_IOC_MAGIC, 4, int)   /* 1 시작, 0 정지 */
#define FAN_SERVO_GET_ANGLE  _IOR(FAN_IOC_MAGIC, 5, int)   /* 30~150 */
#define LCD_CLEAR            _IO(FAN_IOC_MAGIC, 6)
#define LCD_BACKLIGHT        _IOW(FAN_IOC_MAGIC, 7, int)   /* 1 켜기, 0 끄기 */

#endif /* _FAN_IOCTL_H */
