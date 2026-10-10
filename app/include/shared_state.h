#ifndef _SHARED_STATE_H
#define _SHARED_STATE_H

#include <pthread.h>

typedef struct {
    int power_on;    // 0: OFF, 1: ON
    int auto_mode;   // 0: MAN, 1: AUTO
    int fan_level;   // 0 ~ 4
    int swing_on;    // 0: STOP, 1: SWING
    int timer_remain_sec;  // 남은 예약 시간 (초 단위, 0이면 타이머 미설정/해제)
} SystemContext;

// 전역 공유 자원
extern SystemContext g_ctx;
extern pthread_mutex_t g_state_lock;
extern int g_running;

#endif