#ifndef _SHARED_STATE_H
#define _SHARED_STATE_H

#include <pthread.h>

typedef enum {
    MODE_MANUAL = 0, 
    MODE_AUTO        
} FanMode;

typedef enum {
    POWER_OFF = 0, 
    POWER_ON        
} PowerMode;

typedef enum {
    SWING_OFF = 0, 
    SWING_ON        
} SwingMode;

#define FAN_LEVEL_MIN    1
#define FAN_LEVEL_MAX    4

typedef struct {
    PowerMode power;    // 0: OFF, 1: ON
    FanMode mode;   // 0: MAN, 1: AUTO
    int fan_level;   // 1 ~ 4
    SwingMode swing;    // 0: STOP, 1: SWING
    int temp;           // 온도
    int timer_sec;
    int display_visible;
} SystemContext;

// 전역 공유 자원
extern SystemContext g_ctx;
extern pthread_mutex_t g_state_lock;
extern volatile int g_running;
extern int setting_timer;
extern long timer_blink_start_ms;   // 타이머를 마지막으로 돌린 시각 (깜빡임의 시작점)

#endif