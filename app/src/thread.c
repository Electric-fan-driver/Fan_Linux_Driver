#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <string.h>
#include "thread.h"
#include "fan_ioctl.h"
#include "shared_state.h"
#include "fan_api.h"

SystemContext g_ctx = { 0, 0, 0, 0, 0, 0, 0 };
pthread_mutex_t g_state_lock;
volatile int g_running = 1;
int setting_timer =0;
long timer_blink_start_ms = 0;

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
    struct pollfd input = { .fd = fan_input_fd(), .events = POLLIN };
    (void)arg;

    if (input.fd < 0) {
        fprintf(stderr, "[Error] Input device is not open\n");
        g_running = 0;
        return NULL;
    }
    while (g_running) {
        struct fan_input_event ev;
        int ret = poll(&input, 1, 100);

        if (ret < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "[Error] Input poll: %s\n", strerror(errno));
            break;
        }
        if (!ret)
            continue;
        if (input.revents & (POLLHUP | POLLNVAL)) {
            fprintf(stderr, "[Error] Input device disconnected\n");
            break;
        }
        if (!(input.revents & (POLLIN | POLLERR)))
            continue;

        ret = fan_input_get_event(&ev);
        if (ret == -EAGAIN)
            continue;
        if (ret == -EOVERFLOW) {
            fprintf(stderr, "[Error] Input event queue overflow; events lost\n");
            continue;
        }
        if (ret) {
            fprintf(stderr, "[Error] Input read: %s\n", strerror(-ret));
            break;
        }

        pthread_mutex_lock(&g_state_lock);
        fsm_handle_event(&g_ctx, &ev, get_now_ms());
        pthread_mutex_unlock(&g_state_lock);
    }
    g_running = 0;
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
                // 돌린 순간부터 0.25초 보임, 0.25초 꺼짐을 반복 (깜빡임 주기 0.5초)
                g_ctx.display_visible = (((now_ms - timer_blink_start_ms) / 250) % 2 == 0);
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
    

    while (g_running) {
        int temp_raw = 0;
        int ret = bmp180_get_temperature(&temp_raw);
        if (ret == 0) {
            pthread_mutex_lock(&g_state_lock);
            g_ctx.temp = temp_raw;
            fsm_auto_mode(&g_ctx, temp_raw / 10);
        

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
