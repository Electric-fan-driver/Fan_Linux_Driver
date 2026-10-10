#include "thread.h"
#include "shared_state.h"

static SystemContext g_ctx = { 0, 0, 0, 0 };
static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_running = 1;

static ThreadInfo g_threads[] = {
    { "InputThread",  input_thread_fn,  80, 0 },  /* 1순위: 즉각 반응 */
    { "TimerThread",  timer_thread_fn,  50, 0 },  /* 2순위: 0.5초 틱/디스플레이 */
    { "SensorThread", sensor_thread_fn, 20, 0 }   /* 3순위: 백그라운드 온도 측정 */
};

void* input_thread_fn(void* arg){
    /* 스레드 구현 */
}
void* timer_thread_fn(void* arg){
    /* 스레드 구현 */
}
void* sensor_thread_fn(void* arg){
    /* 스레드 구현 */
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