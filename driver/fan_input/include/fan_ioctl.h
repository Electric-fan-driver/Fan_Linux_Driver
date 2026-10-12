/* fan_ioctl.h — 드라이버와 fanctl이 공유. 관리자: D */
#ifndef _FAN_IOCTL_H
#define _FAN_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define FAN_IOCTL_VERSION 1
#define FAN_IOC_MAGIC 'F'

enum { FAN_EV_BTN = 0, FAN_EV_ROT = 1 };
enum { BTN_POWER = 0, BTN_SWING = 1, BTN_ENC_FAN = 2, BTN_ENC_TIMER = 3 };
enum { ENC_FAN = 0, ENC_TIMER = 1 };

struct fan_input_event {
    __u8 type;
    __u8 id;
    __s8 value;
    __u8 _pad;
    __u32 ts_ms;
};

#define FAN_LED_SET_LEVEL   _IOW(FAN_IOC_MAGIC, 1, int)
#define FAN_MOTOR_SET_LEVEL _IOW(FAN_IOC_MAGIC, 2, int)
#define FAN_MOTOR_GET_LEVEL _IOR(FAN_IOC_MAGIC, 3, int)
#define FAN_SERVO_SWING     _IOW(FAN_IOC_MAGIC, 4, int)
#define FAN_SERVO_GET_ANGLE _IOR(FAN_IOC_MAGIC, 5, int)
#define LCD_CLEAR          _IO(FAN_IOC_MAGIC, 6)
#define LCD_BACKLIGHT      _IOW(FAN_IOC_MAGIC, 7, int)
#endif /* _FAN_IOCTL_H */
