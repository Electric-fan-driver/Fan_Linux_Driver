#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/timerfd.h>
#include "fan_api.h"
#include "shared_state.h"
#include "thread.h"

//메인 스레드(전체 시스템 초기화, 종료 시그널(SIGINT) 대기 및 자원 정리, 스레드 생명주기, 우선순위 관리)
// 입력 감지 스레드(엔코더 돌리기, 엔코더 버튼, 전원 스위치, 회전 스위치)
// 타이머 스레드
// 센서스레드(온도)


//공유자원(뮤텍스 걸어야할 것들)
// 전원상태(입력스레드, 센서, 타이머 스레드가 관여)
// 동작 모드( 입력스레드, 센서 스레드가 관여)
// 회전 상태(입력, 타이머 스레드가 관여)
// 풍량 상태


//스레드 별 우선순위 관리
//1순위 : 입력스레드
//2순위 : 타이머 스레드
//3숭뉘 : 센서 스레드



int main(void) {
    int ret;

    fan_input_open();
    led_open();
    bmp180_open();
    lcd_open();
    motor_open();
    servo_open();

    ret = pthread_mutex_init(&g_state_lock, NULL);
    if (ret != 0) {
        return EXIT_FAILURE;
    }

    if (start_all_threads() != 0) {
        pthread_mutex_destroy(&g_state_lock);
        return EXIT_FAILURE;
    }

    wait_all_threads();

    pthread_mutex_destroy(&g_state_lock);
    g_running =0;
    fan_input_close();
    led_close();
    bmp180_close();
    lcd_close();
    motor_close();
    servo_close();

    printf("\n[System] All threads and devices cleanly closed.\n");
    return EXIT_SUCCESS;
}