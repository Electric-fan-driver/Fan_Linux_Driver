#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "fan_ioctl.h"
#include "shared_state.h"
#include "fan_api.h"
int setting_timer =0;
extern int fsm_handle_event(SystemContext *ctx, const struct fan_input_event *ev, long now_ms);

/* --- 만약 별도의 timer.c가 없다면 아래 Mock 타이머가 동작합니다 --- */
#if 1
static int s_mock_timer_sec = 0;
static int s_mock_running = 0;

void timer_reset(void) {
    s_mock_timer_sec = 0;
    s_mock_running = 0;
    printf("  [TIMER] Reset: 0초 (대기 상태)\n");
}

void timer_rotate(int dir, long now_ms) {
    (void)now_ms;
    s_mock_timer_sec += (dir * 60); // 1분 단위 증감9
    if (s_mock_timer_sec < 0) s_mock_timer_sec = 0;
    printf("  [TIMER] 회전 조절: 현재 설정 시간 = %d초\n", s_mock_timer_sec);
}

void timer_button(long now_ms) {
    (void)now_ms;
    if (s_mock_timer_sec > 0) {
        s_mock_running = !s_mock_running;
        printf("  [TIMER] 버튼 클릭: 타이머 %s\n", s_mock_running ? "시작(RUNNING)" : "정지/취소(STOP)");
    } else {
        printf("  [TIMER] 버튼 클릭: 시간이 설정되지 않았습니다.\n");
    }
}

int timer_tick(long now_ms) {
    (void)now_ms;
    if (s_mock_running && s_mock_timer_sec > 0) {
        s_mock_timer_sec -= 5; // 시뮬레이션을 위해 5초씩 빠르게 차감
        if (s_mock_timer_sec <= 0) {
            s_mock_timer_sec = 0;
            s_mock_running = 0;
            return 1; // 1 = 타이머 만료 (전원 자동 종료 유발)
        }
    }
    return 0;
}

void timer_get_display(int *sec, int *visible, long now_ms) {
    (void)now_ms;
    *sec = s_mock_timer_sec;
    *visible = 1;
}
#endif

static long get_mock_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static void print_state(const SystemContext *ctx) {
    printf("\n");
    printf("\n");
    printf("  [상태] 전원: %s | 풍량: %d단 | 모드: %s | 회전: %s | 타이머: %d초 (깜빡임:%d)\n",
           ctx->power ? "ON" : "OFF",
           ctx->fan_level,
           ctx->mode ? "AUTO" : "MANUAL",
           ctx->swing ? "ON" : "OFF",
           ctx->timer_sec,
           ctx->display_visible);
}

int main(void) {
    SystemContext ctx = {0};
    struct fan_input_event ev;
    int choice;

    printf("====================================================\n");
    printf("   스마트 선풍기 FSM & 타이머 시뮬레이션 검증기     \n");
    printf("====================================================\n");
    print_state(&ctx);

    while (1) {
        printf("\n[메뉴]\n");
        printf("  1: 전원 버튼\n");
        printf("  2: 풍량 엔코더 +1     | 3: 풍량 엔코더 -1\n");
        printf("  4: 모드 버튼          | 5: 회전 버튼\n");
        printf("  6: 타이머 엔코더 +1   | 7: 타이머 엔코더 -1\n");
        printf("  8: 타이머 버튼        | 9: 시간 경과 (Timer Tick 시뮬레이션)\n");
        printf("  0: 종료\n");
        printf("선택: ");

        if (scanf("%d", &choice) != 1 || choice == 0) break;

        long now = get_mock_time();
        int handled = 0;

        switch (choice) {
            case 1:
                ev.type = FAN_EV_BTN; ev.id = BTN_POWER; ev.value = 1;
                handled = fsm_handle_event(&ctx, &ev, now);
                break;
            case 2:
                ev.type = FAN_EV_ROT; ev.id = ENC_FAN; ev.value = 1;
                handled = fsm_handle_event(&ctx, &ev, now);
                break;
            case 3:
                ev.type = FAN_EV_ROT; ev.id = ENC_FAN; ev.value = -1;
                handled = fsm_handle_event(&ctx, &ev, now);
                break;
            case 4:
                ev.type = FAN_EV_BTN; ev.id = BTN_ENC_FAN; ev.value = 1;
                handled = fsm_handle_event(&ctx, &ev, now);
                break;
            case 5:
                ev.type = FAN_EV_BTN; ev.id = BTN_SWING; ev.value = 1;
                handled = fsm_handle_event(&ctx, &ev, now);
                break;
            case 6: // 타이머 엔코더 +1
                ev.type = FAN_EV_ROT; ev.id = ENC_TIMER; ev.value = 1;
                handled = fsm_handle_event(&ctx, &ev, now);
                break;
            case 7: // 타이머 엔코더 -1
                ev.type = FAN_EV_ROT; ev.id = ENC_TIMER; ev.value = -1;
                handled = fsm_handle_event(&ctx, &ev, now);
                break;
            case 8: // 타이머 버튼
                ev.type = FAN_EV_BTN; ev.id = BTN_ENC_TIMER; ev.value = 1;
                handled = fsm_handle_event(&ctx, &ev, now);
                break;
            case 9: // 타이머 틱(시간 경과) 처리
                if (ctx.power) {
                    int expired = timer_tick(now);
                    if (expired) {
                        printf("  >>> [알림] 타이머 만료! 전원이 자동으로 꺼집니다. <<<\n");
                        ctx.power = 0;
                        ctx.fan_level = 0;
                        ctx.swing = 0;
                        ctx.mode = 0;
                    }
                    timer_get_display(&ctx.timer_sec, &ctx.display_visible, now);
                    if (setting_timer) {
                        ctx.display_visible = ((now / 500) % 2 == 0);
                    }
                    handled = 1;
                } else {
                    printf("  전원이 꺼져 있어 타이머 틱이 동작하지 않습니다.\n");
                }
                break;
            default:
                printf("잘못된 선택입니다.\n");
                continue;
        }

        // FSM 이벤트 처리 후 타이머 디스플레이 정보 최신화
        if (choice != 9 && ctx.power) {
            timer_get_display(&ctx.timer_sec, &ctx.display_visible, now);
        }

        printf("-> 이벤트 처리: %s\n", handled ? "성공/반영" : "무시됨");
        print_state(&ctx);
    }

    printf("프로그램을 종료합니다.\n");
    return 0;
}