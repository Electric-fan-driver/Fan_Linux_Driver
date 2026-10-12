#include <unistd.h>
#include "thread.h"
#include "fan_ioctl.h"
#include "shared_state.h"
#include "fan_api.h"

SystemContext g_ctx = { 0, 0, 0, 0, 0, 0, 0 };
pthread_mutex_t g_state_lock;
volatile int g_running = 1;
int setting_timer =0;

extern int fsm_handle_event(SystemContext *ctx, const struct fan_input_event *ev, long now_ms);
extern void apply_to_hardware(const SystemContext *copy);
extern int fsm_auto_mode(SystemContext *ctx, int current_temp);

static ThreadInfo g_threads[] = {
    { "InputThread",  input_thread_fn,  80, 0 },  /* 1순위: 입력 스레드 */
    { "TimerThread",  timer_thread_fn,  50, 0 },  /* 2순위: 0.5초 틱/디스플레이 */
    { "SensorThread", sensor_thread_fn, 20, 0 }   /* 3순위: 백그라운드 온도 측정 */
};

static long get_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

void* input_thread_fn(void* arg){
    //역할 : 버튼 엔코더 회전 이벤트 감지해서 상태변경
    /* 스레드 구현 */
    struct fan_input_event ev;// 내가 버튼 누르면 8바이트 입력 이벤트 구조체로 전달 
    SystemContext copy_run_value;// 공유자원 변경한 거를 공유자원은 그대로 두고 복사해서 하드웨어에 넘기기 

    while(g_running){
        long now_ms = get_now_ms();

        pthread_mutex_lock(&g_state_lock); // 상태 확인했으니 공유 자원 접근

        fsm_handle_event(&g_ctx, &ev, now_ms);

        copy_run_value = g_ctx;

        pthread_mutex_unlock(&g_state_lock); // 락 해제

    }
    return NULL;
}
void* timer_thread_fn(void* arg) {
    SystemContext copy_run_value;

    while (g_running) {
        int hw_flag = 1;
        long now_ms = get_now_ms();

        pthread_mutex_lock(&g_state_lock);

        if (g_ctx.power) {
            int ret = timer_tick(now_ms);

            if (ret == 1) {
                g_ctx.power = 0;
                g_ctx.fan_level = 0;
                g_ctx.swing = 0;
                g_ctx.mode = 0;
                hw_flag = 0;
            }

            timer_get_display(&g_ctx.timer_sec, &g_ctx.display_visible, now_ms);
            if (setting_timer) {
                g_ctx.display_visible = ((now_ms / 500) % 2 == 0);
            }
        }

        copy_run_value = g_ctx;

        pthread_mutex_unlock(&g_state_lock);

        /* 하드웨어 반영 */
        if(hw_flag) apply_to_hardware(&copy_run_value);

        /* 100ms 대기 (I/O 폭주 및 CPU 100% 점유 방지) */
        usleep(100000);
    }

    return NULL;
}

void* sensor_thread_fn(void* arg) {
    SystemContext copy_run_value;

    while (g_running) {
        int temp_raw = 0;
        int ret = bmp180_get_temperature(&temp_raw);
        if (ret == 0) {
            pthread_mutex_lock(&g_state_lock);
            g_ctx.temp = temp_raw;
            int changed = fsm_auto_mode(&g_ctx, temp_raw / 10);
            copy_run_value = g_ctx;

            pthread_mutex_unlock(&g_state_lock);
        }
        sleep(1);
    }
    return NULL;
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
            fprintf(stderr, "[Error] %s 생성 실패\n", g_threads[i].name);
            return -1;
        }
    }
    return 0;
}

void wait_all_threads(void) {
    for (size_t i = 0; i < NUM_THREADS; i++) {
        pthread_join(g_threads[i].tid, NULL);
    }
}