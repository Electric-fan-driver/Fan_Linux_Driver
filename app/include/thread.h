#include <pthread.h>
#include <sched.h>
#include <stdio.h>

/* 스레드 메타데이터 구조체 */
typedef struct {
    const char *name;          /* 디버깅용 스레드 이름 */
    void *(*func)(void *);     /* 실행할 스레드 함수 */
    int priority;              /* RT 우선순위 (SCHED_FIFO: 1~99) */
    pthread_t tid;             /* 생성 후 발급받을 스레드 ID */
} ThreadInfo;

// 임시 스레드 함수 프로토타입
void* input_thread_fn(void* arg);
void* timer_thread_fn(void* arg);
void* sensor_thread_fn(void* arg);

//스레드 목록 테이블 정의
extern static ThreadInfo g_threads[];
extern int start_all_threads(void);
extern void wait_all_threads(void);