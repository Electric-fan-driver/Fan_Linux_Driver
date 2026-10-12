#include "fan_ioctl.h"
#include "shared_state.h"

void timer_reset(void);
void timer_button(long now_ms);
void timer_rotate(int dir, long now_ms);
extern void fan_temp_policy(int * target_level, int current_temp);

extern int setting_timer;

/* --- 버튼 핸들러 --- */

static int handle_power_button(SystemContext *ctx) {
    ctx->power = !ctx->power;
    if (!ctx->power) {
        ctx->fan_level = 0;
        ctx->swing = SWING_OFF;
        ctx->mode = MODE_MANUAL;
        timer_reset();
    } else {
        ctx->fan_level = FAN_LEVEL_MIN;
    }
    return 1;
}

static int handle_swing_button(SystemContext *ctx) {
    ctx->swing = !ctx->swing;
    return 1;
}

static int handle_mode_button(SystemContext *ctx) {
    ctx->mode = !ctx->mode;
    return 1;
}

static int handle_timer_button(SystemContext *ctx, long now_ms) {
    setting_timer = 0;
    timer_button(now_ms);
    return 1;
}

/* --- 엔코더 회전 핸들러 --- */

// 풍량 엔코더 회전: 1~4단 조절 (자동 모드일 때는 수동 변경 방지)
static int handle_fan_rotate(SystemContext *ctx, int delta) {
    if (ctx->mode) {
        return 0; // 자동 모드에서는 풍량 수동 조절 무시
    }

    int prev_level = ctx->fan_level;
    ctx->fan_level += delta;

    // 풍량 범위 클램핑 (1단 ~ 4단)
    if (ctx->fan_level < FAN_LEVEL_MIN) ctx->fan_level = FAN_LEVEL_MIN;
    if (ctx->fan_level > FAN_LEVEL_MAX) ctx->fan_level = FAN_LEVEL_MAX;

    return (ctx->fan_level != prev_level); // 실제 값이 변했을 때만 1 반환
}

// 타이머 엔코더 회전
static int handle_timer_rotate(int delta, long now_ms) {
    setting_timer = 1;
    timer_rotate(delta, now_ms);
    return 1;
}

/* --- 메인 이벤트 라우터 --- */

int fsm_handle_event(SystemContext *ctx, const struct fan_input_event *ev, long now_ms) {
    // 1. 버튼 이벤트 처리
    if (ev->type == FAN_EV_BTN && ev->value == 1) {
        if (ev->id == BTN_POWER) {
            return handle_power_button(ctx);
        }

        // 전원이 꺼져 있으면 전원 버튼 외 모든 입력 무시
        if (!ctx->power) {
            return 0;
        }

        switch (ev->id) {
            case BTN_SWING:      return handle_swing_button(ctx);
            case BTN_ENC_FAN:    return handle_mode_button(ctx);
            case BTN_ENC_TIMER:  return handle_timer_button(ctx, now_ms);
            default:             return 0;
        }
    }

    // 2. 엔코더 회전 이벤트 처리
    else if (ev->type == FAN_EV_ROT) {
        // 전원이 꺼져 있으면 회전 입력 무시
        if (!ctx->power) {
            return 0;
        }

        switch (ev->id) {
            case ENC_FAN:
                // ev->value가 +1(시계방향, 증가), -1(반시계방향, 감소)
                return handle_fan_rotate(ctx, ev->value);

            case ENC_TIMER:
                return handle_timer_rotate(ev->value, now_ms);

            default:
                return 0;
        }
    }

    return 0;
}

int fsm_auto_mode(SystemContext *ctx, int current_temp){

    if (ctx->power && ctx->mode) {
        int target_level = ctx->fan_level;

        /* 온도 구간별 풍량 단계 결정 */
        fan_temp_policy(&target_level, current_temp);

        if (ctx->fan_level != target_level) {
            ctx->fan_level = target_level;
            return 1;
        }
    }
    return 0;
}