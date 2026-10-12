// test_full.c - 전체 시험 프로그램 (D 의 main 이 나오기 전에 전체 동작을 확인하는 용도)
//
// 설계서의 FSM 을 그대로 따른다
//   - 전원 버튼: ON/OFF. OFF 면 모터,LED 꺼짐, 서보 그 자리 정지, 전원 버튼 말고는 모두 무시
//                ON 이 되면 전부 처음 상태로 (MAN, 풍량 0, 회전 꺼짐, 타이머 0)
//   - 풍량 엔코더: MAN 에서 돌리면 풍량 0~4, 누르면 MAN <-> AUTO (AUTO->MAN 이면 풍량 0)
//   - 회전 버튼: 좌우 회전 켜기/끄기
//   - 타이머 엔코더: 돌리면 10분 단위 설정(깜빡임), 누르거나 10초 지나면 확정 후 카운트다운,
//                    동작 중 누르면 취소, 동작 중 돌리면 일시정지 후 다시 설정, 끝나면 전원 OFF
//   - AUTO: 온도 26/29/32/35도 기준으로 0~4단계, 0.5도 히스테리시스 + 3초 유지 후 바뀜
//   - 온도: BMP180 을 1초마다 읽음. 키보드 t 30.5 로 온도를 직접 넣을 수도 있고, t s 로 센서로 돌아감
//   - LCD : 상태가 바뀔 때만 해당 칸을 고쳐 씀. 전원 OFF 면 화면을 지우고 백라이트를 끔
//   - BMP180 이나 LCD 가 없으면 그 부분만 건너뛰고 나머지는 그대로 동작함
//
// 필요한 파일 (이 브랜치에는 led.c, fan_led_driver.c 만 들어 있음)
//   app/src/motor.c, driver/fan_motor_driver.c, driver/fan_servo_driver.c : feat/motor-servo-driver
//   app/src/bmp180.c, app/src/display.c, driver/fan_bmp180_*, driver/fan_lcd_* : integration/fan-drivers
//   app/src/input.c 와 입력 드라이버(전원,회전 버튼, 엔코더 2개, /dev/fan_input 에서 8바이트 이벤트): 아직 팀 저장소에 없음
//
// 빌드 (저장소 맨 위 폴더에서, 젯슨):
//   gcc -Wall -Wextra -Iinclude -o test_full app/tests/test_full.c app/src/motor.c app/src/led.c app/src/input.c app/src/bmp180.c app/src/display.c
//
// 실행 (드라이버를 모두 올린 뒤):  sudo ./test_full   또는   sudo ./test_full --fast
//   끝내기: q (Enter) 또는 Ctrl+C
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

#include "fan_api.h"

#define TICK_MS			500	// 0.5초마다 타이머,AUTO,화면 갱신
#define LEVEL_MAX		4
#define TIMER_MAX_UNITS		72	// 12시간 = 10분 × 72
#define TIMER_CONFIRM_MS	10000	// 설정 중 10초 동안 안 돌리면 자동 확정
#define AUTO_HOLD_MS		3000	// AUTO: 새 단계가 3초 이어져야 바꿈
#define AUTO_HYST_X10		5	// AUTO: 내려갈 때는 기준보다 0.5℃ 더 내려가야 함

static volatile sig_atomic_t quit;

static void on_sigint(int sig)
{
	(void)sig;
	quit = 1;
}

static long now_ms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

// 상태 줄을 지우고 메시지 한 줄을 찍는다 (상태 줄은 뒤에서 다시 그림)
static void logmsg(const char *fmt, ...)
{
	va_list ap;

	printf("\r\033[K> ");
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

// ===================== 상태 =====================

static bool power_on;
static bool auto_mode;
static bool swing;
static int level;			// 지금 풍량 0~4
static int temp_x10 = 245;		// 지금 온도 (246 = 24.6℃)
static bool temp_valid = true;		// 센서 읽기가 성공했는지
static bool sim_temp;			// true 면 키보드로 넣은 온도를 씀
static bool have_bmp, have_lcd;		// 센서,LCD 를 열었는지
static int auto_cand = -1;		// AUTO: 바뀌려고 기다리는 단계 (-1 = 없음)
static long auto_cand_since;		// AUTO: 그 단계가 처음 나온 시각
static int unit_sec = 600;		// 타이머 한 칸 (10분, --fast 면 10초)

// ===================== 타이머 하위 FSM =====================

enum tstate { T_IDLE, T_SETTING, T_RUNNING };
static enum tstate tst;
static int t_value;			// SETTING: 설정값 (초)
static long t_deadline;			// RUNNING: 끝나는 시각 (ms)
static long t_last_rot;			// SETTING: 마지막으로 돌린 시각 (ms)

static int t_max(void)
{
	return TIMER_MAX_UNITS * unit_sec;
}

// 지금 값보다 큰 가장 작은 한 칸 배수 / 작은 가장 큰 한 칸 배수
static int t_next(int v)
{
	int n = (v / unit_sec + 1) * unit_sec;

	return n > t_max() ? t_max() : n;
}

static int t_prev(int v)
{
	return v > 0 ? ((v - 1) / unit_sec) * unit_sec : 0;
}

static int t_remaining(long now)	// RUNNING: 남은 초 (올림)
{
	long ms = t_deadline - now;

	return ms <= 0 ? 0 : (int)((ms + 999) / 1000);
}

static void timer_reset_local(void)
{
	tst = T_IDLE;
	t_value = 0;
}

static void timer_rotate_local(int dir, long now)
{
	switch (tst) {
	case T_IDLE:
		if (dir < 0)
			return;			// IDLE 에서 왼쪽은 무시
		t_value = unit_sec;
		break;
	case T_SETTING:
		t_value = dir > 0 ? t_next(t_value) : t_prev(t_value);
		break;
	case T_RUNNING:			// 일시정지하고 남은 시간 기준으로 다시 설정
		t_value = t_remaining(now);
		t_value = dir > 0 ? t_next(t_value) : t_prev(t_value);
		break;
	}
	tst = T_SETTING;
	t_last_rot = now;
}

static void timer_confirm(long now)
{
	if (t_value > 0) {
		tst = T_RUNNING;
		t_deadline = now + t_value * 1000L;
		logmsg("타이머 시작 (%d분 %d초)", t_value / 60, t_value % 60);
	} else {
		tst = T_IDLE;
		logmsg("타이머 0 → 해제");
	}
}

static void timer_button_local(long now)
{
	if (tst == T_SETTING) {
		timer_confirm(now);
	} else if (tst == T_RUNNING) {
		timer_reset_local();
		logmsg("타이머 취소");
	}
}

// 1 = 방금 끝남
static int timer_tick_local(long now)
{
	if (tst == T_SETTING && now - t_last_rot >= TIMER_CONFIRM_MS) {
		logmsg("10초 동안 안 돌려서 자동 확정");
		timer_confirm(now);
	} else if (tst == T_RUNNING && now >= t_deadline) {
		timer_reset_local();
		return 1;
	}
	return 0;
}

static void timer_display(int *sec, int *visible, long now)
{
	switch (tst) {
	case T_IDLE:
	default:
		*sec = 0;
		*visible = 1;
		break;
	case T_SETTING:			// 마지막으로 돌린 뒤 0.5초마다 보임/숨김
		*sec = t_value;
		*visible = ((now - t_last_rot) / 500) % 2 == 0;
		break;
	case T_RUNNING:
		*sec = t_remaining(now);
		*visible = 1;
		break;
	}
}

// ===================== AUTO =====================

static const int auto_thr[LEVEL_MAX] = { 260, 290, 320, 350 };	// 26 / 29 / 32 / 35 ℃

// 히스테리시스 없이 온도만으로 정한 단계 (MAN → AUTO 로 바꿀 때 바로 적용)
static int plain_level(int t)
{
	int lv = 0;

	while (lv < LEVEL_MAX && t > auto_thr[lv])
		lv++;
	return lv;
}

// 올라갈 때는 기준을 넘어야, 내려갈 때는 기준보다 0.5℃ 더 내려가야 바뀜
static int hyst_level(int cur, int t)
{
	int lv = cur;

	while (lv < LEVEL_MAX && t > auto_thr[lv])
		lv++;
	while (lv > 0 && t <= auto_thr[lv - 1] - AUTO_HYST_X10)
		lv--;
	return lv;
}

static void auto_update(long now)
{
	int target = hyst_level(level, temp_x10);

	if (target == level) {
		auto_cand = -1;
		return;
	}
	if (target != auto_cand) {		// 새 후보: 지금부터 3초 재기 시작
		auto_cand = target;
		auto_cand_since = now;
		return;
	}
	if (now - auto_cand_since >= AUTO_HOLD_MS) {
		logmsg("AUTO: %d.%d℃ 가 3초 이어져서 풍량 %d → %d",
		       temp_x10 / 10, temp_x10 % 10, level, target);
		level = target;
		auto_cand = -1;
	}
}

// ===================== 출력 =====================

// 바뀐 것만 드라이버에 보낸다
static void apply(void)
{
	static int last_lv = -1, last_sw = -1;
	int lv = power_on ? level : 0;
	int sw = power_on && swing;

	if (lv != last_lv) {
		motor_set_level(lv);
		led_set_level(lv);
		last_lv = lv;
	}
	if (sw != last_sw) {
		servo_set_swing(sw);
		last_sw = sw;
	}
}

// BMP180 에서 온도를 읽는다. 키보드로 넣은 온도를 쓰는 중이면 건너뜀
static void sensor_update(void)
{
	static int last_ok = -1;
	int t, ok;

	if (!have_bmp || sim_temp)
		return;
	ok = bmp180_get_temperature(&t) == 0;
	if (ok) {
		temp_x10 = t;
		temp_valid = true;
	} else {
		temp_valid = false;
	}
	if (ok != last_ok) {
		logmsg(ok ? "BMP180 읽기 정상" : "BMP180 읽기 실패");
		last_ok = ok;
	}
}

// LCD 를 상태에 맞춰 고친다. 바뀐 칸만 쓴다
static void lcd_update(long now)
{
	static int l_power = -1, l_auto = -1, l_rot = -1, l_sec = -1, l_vis = -1, l_temp = 99999, l_valid = -1;
	int sec, vis;

	if (!have_lcd)
		return;
	if (power_on != l_power) {
		l_power = power_on;
		l_auto = l_rot = l_sec = l_vis = l_valid = -1;	// 전부 다시 그림
		l_temp = 99999;
		lcd_clear();
		lcd_set_backlight(power_on ? 1 : 0);
	}
	if (!power_on)
		return;
	if (auto_mode != l_auto) {
		lcd_show_mode(auto_mode);
		l_auto = auto_mode;
	}
	timer_display(&sec, &vis, now);
	if (sec != l_sec || vis != l_vis) {
		lcd_show_timer(sec, vis);
		l_sec = sec;
		l_vis = vis;
	}
	if (swing != l_rot) {
		lcd_show_rot(swing);
		l_rot = swing;
	}
	if (temp_x10 != l_temp || temp_valid != l_valid) {
		lcd_show_temp(temp_x10, temp_valid);
		l_temp = temp_x10;
		l_valid = temp_valid;
	}
}

// 터미널에 상태 한 줄 (LCD 와 같은 내용)
static void status_line(long now)
{
	char tbuf[16];
	int sec, vis;

	timer_display(&sec, &vis, now);
	if (vis)
		snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", sec / 3600, sec / 60 % 60, sec % 60);
	else
		snprintf(tbuf, sizeof(tbuf), "        ");

	printf("\r\033[K[%s] %s  풍량 %d  회전 %s  타이머 %s %s  온도 %d.%d℃ %s",
	       power_on ? "ON " : "OFF", auto_mode ? "AUTO" : "MAN ",
	       power_on ? level : 0, power_on && swing ? "ON " : "OFF", tbuf,
	       tst == T_SETTING ? "(설정 중)" : tst == T_RUNNING ? "(동작 중)" : "        ",
	       temp_x10 / 10, temp_x10 % 10,
	       sim_temp ? "(키보드)" : !have_bmp ? "(센서 없음)" : temp_valid ? "(센서)" : "(센서 오류)");
	fflush(stdout);
}

// ===================== 입력 처리 =====================

static void power_set(bool on)
{
	power_on = on;
	auto_mode = false;			// ON/OFF 모두 처음 상태로
	level = 0;
	swing = false;
	auto_cand = -1;
	timer_reset_local();
	logmsg("전원 %s", on ? "ON" : "OFF");
}

static void handle_event(const struct fan_input_event *ev, long now)
{
	if (ev->type == FAN_EV_BTN && ev->id == BTN_POWER) {
		power_set(!power_on);
		return;
	}
	if (!power_on)
		return;				// OFF 에서는 전원 버튼 말고 모두 무시

	if (ev->type == FAN_EV_BTN) {
		switch (ev->id) {
		case BTN_SWING:
			swing = !swing;
			logmsg("회전 %s", swing ? "ON" : "OFF");
			break;
		case BTN_ENC_FAN:		// MAN ↔ AUTO
			auto_mode = !auto_mode;
			auto_cand = -1;
			level = auto_mode ? plain_level(temp_x10) : 0;
			logmsg("%s 모드 (풍량 %d)", auto_mode ? "AUTO" : "MAN", level);
			break;
		case BTN_ENC_TIMER:
			timer_button_local(now);
			break;
		}
	} else if (ev->type == FAN_EV_ROT) {
		if (ev->id == ENC_FAN) {
			if (auto_mode)
				return;		// AUTO 에서는 풍량 엔코더 무시
			level += ev->value;
			if (level < 0)
				level = 0;
			if (level > LEVEL_MAX)
				level = LEVEL_MAX;
		} else if (ev->id == ENC_TIMER) {
			timer_rotate_local(ev->value, now);
		}
	}
}

static void on_tick(long now)
{
	static int ticks;

	if (++ticks % 2 == 0)			// 0.5초 틱 두 번 = 1초마다 센서 읽기
		sensor_update();
	if (!power_on)
		return;
	if (timer_tick_local(now)) {
		logmsg("타이머 끝 → 전원 OFF");
		power_set(false);
		return;
	}
	if (auto_mode)
		auto_update(now);
}

// 키보드: "t 30.5" = 온도, "q" = 끝
static bool handle_stdin(void)
{
	char line[64];
	char c;
	double t;
	ssize_t n = read(STDIN_FILENO, line, sizeof(line) - 1);

	if (n <= 0)
		return false;			// 입력이 닫힘 → 더 이상 안 봄
	line[n] = '\0';
	if (line[0] == 'q') {
		quit = 1;
	} else if (line[0] == 't' && sscanf(line + 1, " %c", &c) == 1 && c == 's') {
		sim_temp = false;
		sensor_update();
		logmsg("센서 온도로 돌아감");
	} else if (line[0] == 't' && sscanf(line + 1, "%lf", &t) == 1) {
		sim_temp = true;
		temp_valid = true;
		temp_x10 = (int)(t * 10 + (t >= 0 ? 0.5 : -0.5));
		logmsg("키보드 온도 %d.%d℃ (센서로 돌아가려면 t s)", temp_x10 / 10, temp_x10 % 10);
	} else {
		logmsg("명령: t <온도> (예: t 30.5), t s = 센서로 돌아감, q = 끝");
	}
	return true;
}

// ===================== main =====================

int main(int argc, char *argv[])
{
	struct epoll_event e = { .events = EPOLLIN }, evs[4];
	struct itimerspec its = {
		.it_interval = { 0, TICK_MS * 1000000L },
		.it_value = { 0, TICK_MS * 1000000L },
	};
	struct sigaction sa = { .sa_handler = on_sigint };	// SA_RESTART 없음 → epoll_wait 가 깨어남
	struct fan_input_event ev;
	int ep, tfd, ret, n, i;
	uint64_t expirations;

	if (argc > 1 && strcmp(argv[1], "--fast") == 0)
		unit_sec = 10;
	sigaction(SIGINT, &sa, NULL);

	// 장치 4개 열기
	if ((ret = fan_input_open()) < 0 || (ret = motor_open()) < 0 ||
	    (ret = servo_open()) < 0 || (ret = led_open()) < 0) {
		printf("장치 열기 실패: %s\n드라이버 4개가 올라가 있나요? (lsmod | grep fan_)\n", strerror(-ret));
		goto out;
	}

	have_bmp = bmp180_open() == 0;
	have_lcd = lcd_open() == 0;
	logmsg("BMP180 %s, LCD %s", have_bmp ? "연결됨" : "없음(건너뜀)", have_lcd ? "연결됨" : "없음(건너뜀)");
	sensor_update();

	tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	timerfd_settime(tfd, 0, &its, NULL);

	// epoll: 입력 장치, 0.5초 타이머, 키보드를 한꺼번에 기다림
	ep = epoll_create1(EPOLL_CLOEXEC);
	e.data.fd = fan_input_fd();
	epoll_ctl(ep, EPOLL_CTL_ADD, e.data.fd, &e);
	e.data.fd = tfd;
	epoll_ctl(ep, EPOLL_CTL_ADD, tfd, &e);
	e.data.fd = STDIN_FILENO;
	epoll_ctl(ep, EPOLL_CTL_ADD, STDIN_FILENO, &e);

	printf("스마트 선풍기 전체 시험 프로그램%s\n", unit_sec == 10 ? " (타이머 빠르게: 10분 → 10초)" : "");
	printf("  전원 버튼으로 켜고, 엔코더,버튼을 써 보세요.\n");
	printf("  키보드: t 30.5 = 온도 직접 넣기, t s = 센서로 돌아감,  q = 끝\n\n");

	power_on = false;
	apply();
	lcd_update(now_ms());
	status_line(now_ms());

	while (!quit) {
		n = epoll_wait(ep, evs, 4, -1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			perror("epoll_wait");
			break;
		}
		for (i = 0; i < n; i++) {
			long now = now_ms();
			int fd = evs[i].data.fd;

			if (fd == fan_input_fd()) {
				while (fan_input_get_event(&ev) == 0)	// 쌓인 입력을 모두 처리
					handle_event(&ev, now);
			} else if (fd == tfd) {
				if (read(tfd, &expirations, sizeof(expirations)) > 0)
					on_tick(now);
			} else if (fd == STDIN_FILENO) {
				if (!handle_stdin())
					epoll_ctl(ep, EPOLL_CTL_DEL, STDIN_FILENO, NULL);
			}
		}
		apply();
		lcd_update(now_ms());
		status_line(now_ms());
	}

	printf("\n끝냅니다. 모터,LED 를 끄고 회전을 멈춥니다.\n");
	power_on = false;
	apply();
	lcd_update(now_ms());			// 화면을 지우고 백라이트를 끔
	close(ep);
	close(tfd);
out:
	if (have_lcd)
		lcd_close();
	if (have_bmp)
		bmp180_close();
	led_close();
	servo_close();
	motor_close();
	fan_input_close();
	return 0;
}
