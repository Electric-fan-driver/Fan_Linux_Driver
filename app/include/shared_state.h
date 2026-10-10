#ifndef _SHARED_STATE_H
#define _SHARED_STATE_H

#include <pthread.h>

typedef struct {
    int power_on;    // 0: OFF, 1: ON
    int auto_mode;   // 0: MAN, 1: AUTO
    int fan_level;   // 0 ~ 4
    int swing_on;    // 0: STOP, 1: SWING
} SystemContext;

// 전역 공유 자원
extern static SystemContext g_ctx;
extern static pthread_mutex_t g_state_lock;
extern static int g_running;

#endif