#include "thread.h"
#include "shared_state.h"

SystemContext g_ctx = { 0, 0, 0, 0 };
pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;
volatile int g_running = 1;

static ThreadInfo g_threads[] = {
    { "InputThread",  input_thread_fn,  80, 0 },  /* 1순위: 즉각 반응 */
    { "TimerThread",  timer_thread_fn,  50, 0 },  /* 2순위: 0.5초 틱/디스플레이 */
    { "SensorThread", sensor_thread_fn, 20, 0 }   /* 3순위: 백그라운드 온도 측정 */
};

void* input_thread_fn(void* arg){
    //역할 : 버튼 엔코더 회전 이벤트 감지해서 상태변경
    /* 스레드 구현 */
    struct fan_input_event ev;// 내가 버튼 누르면 8바이트 입력 이벤트 구조체로 전달 
    SystemContext copy_run_value;// 공유자원 변경한 거를 공유자원은 그대로 두고 복사해서 하드웨어에 넘기기 
    int changed = 0;

    while(g_running){
        int ret = fan_input_get_event(&ev);
        if (ret == -EAGAIN || ret < 0) {
            usleep(10000);
            continue;
        }

        pthread_mutex_lock(&g_state_lock); // 상태 확인했으니 공유자원 접근

        /* 버튼 이벤트 처리 */
        if (ev.type == FAN_EV_BTN && ev.value == 1) {
            if (ev.id == BTN_POWER) {
                g_ctx.power_on = !g_ctx.power_on;
                if (!g_ctx.power_on) {
                    // 전원 꺼짐 시 리셋
                    g_ctx.fan_level = 0;
                    g_ctx.swing_on = 0;
                    g_ctx.auto_mode = 0;
                } else {
                    g_ctx.fan_level = 1; // 켜질 때 기본 1단
                }
                changed = 1;
            }
            else if (g_ctx.power_on) {
                // 전원이 켜져 있을 때만 동작하는 버튼들
                if (ev.id == BTN_SWING) {
                    g_ctx.swing_on = !g_ctx.swing_on;
                    changed = 1;
                }
                else if (ev.id == BTN_ENC_FAN) {
                    // 풍량 엔코더 버튼 -> 모드 전환(수동/자동)으로 활용 시
                    g_ctx.auto_mode = !g_ctx.auto_mode;
                    changed = 1;
                }
                else if (ev.id == BTN_ENC_TIMER) {
                    // 타이머 버튼 -> 20분(1200초) 추가
                    g_ctx.timer_remain_sec += (20 * 60);
                    changed = 1;
                }
            }
        }

        /*엔코더 회전 이벤트 처리 */
        else if (ev.type == FAN_EV_ROT && g_ctx.power_on) {
            if (ev.id == ENC_FAN && !g_ctx.auto_mode) {
                // 풍량 엔코더 회전 (ev.value: +1 CW, -1 CCW)
                g_ctx.fan_level += ev.value;
                if (g_ctx.fan_level < 1) g_ctx.fan_level = 1;
                if (g_ctx.fan_level > 4) g_ctx.fan_level = 4;
                changed = 1;
            }
        }
        copy_run_value = g_ctx;
        pthread_mutex_unlock(&g_state_lock); // 락 해제

        if (changed) {
            apply_to_hardware(&copy_run_value);
        }
    }
}
void* timer_thread_fn(void* arg){
    // 역할: 
    /* 스레드 구현 */
    while(g_running){
        
    }
}
void* sensor_thread_fn(void* arg){
    /* 스레드 구현 */
    while(g_running){
        
    }
}

#define NUM_THREADS (sizeof(g_threads) / sizeof(g_threads[0]))

//임시 스레드 생성 함수
int start_all_threads(void) {
    for (size_t i = 0; i < NUM_THREADS; i++) {
        pthread_attr_t attr;
        struct sched_param param;

        pthread_attr_init(&attr);
        pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
        pthread_attr_setschedpolicy(&attr, SCHED_FIFO);

        param.sched_priority = g_threads[i].priority;
        pthread_attr_setschedparam(&attr, &param);

        // 생성과 동시에 g_threads[i].tid에 저장
        int ret = pthread_create(&g_threads[i].tid, &attr, g_threads[i].func, NULL);
        pthread_attr_destroy(&attr);

        if (ret != 0) {
            fprintf(stderr, "[Error] %s 생성 실패 (root 권한 확인)\n", g_threads[i].name);
            return -1;
        }

        printf("[Main] %s 시작됨 (TID: %lu, Priority: %d)\n",
               g_threads[i].name, (unsigned long)g_threads[i].tid, g_threads[i].priority);
    }
    return 0;
}

void wait_all_threads(void) {
    for (size_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(g_threads[i].tid, NULL);
        printf("[Main] %s 종료 완료\n", g_threads[i].name);
    }
}