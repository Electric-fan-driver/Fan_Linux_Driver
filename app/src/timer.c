#include "fan_api.h"

static int set_time;
static int remaining_sec;// 현재 남은 시간 (초 단위)
static int is_setting_mode;// 엔코더를 돌리거나 버튼을 눌러 시간 설정 중인지 여부 (1: 설정 중, 0: 카운트다운 진행 중)
static long last_interaction_ms;// 마지막으로 버튼/엔코더 조작이 발생한 시점 (now_ms 기준, 예: 3~5초간 추가 조작 없으면 설정 확정)
static long last_tick_ms;// 1초 경과를 계산하기 위한 마지막 틱 시점

void timer_reset(void){

}
void timer_rotate(int dir, long now_ms){

}
void timer_button(long now_ms){

}
int  timer_tick(long now_ms){

}
void timer_get_display(int *sec, int *visible, long now_ms){

}