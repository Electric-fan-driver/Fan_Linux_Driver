#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "fan_input_event.h"

#define FAN_DEVICE_PATH "/dev/fandev"

/* 
 * 앱 레벨 이벤트 타입 및 ID 폴백 정의 
 * (app/include 쪽 헤더에 이미 정의되어 있으면 그 정의를 그대로 사용합니다)
 */
#ifndef FAN_EV_ROT
#define FAN_EV_ROT 1
#endif
#ifndef FAN_EV_BTN
#define FAN_EV_BTN 2
#endif

#ifndef FAN_ENCODER_ID
#define FAN_ENCODER_ID 1
#endif
#ifndef FAN_BUTTON_ID
#define FAN_BUTTON_ID  2
#endif

/* 
 * 앱 측 struct fan_input_event 기본 구조 선언
 * 헤더에서 이미 선언된 경우 충돌하지 않도록 필드 규격을 맞춥니다.
 */
struct fan_input_event {
    int type;   /* FAN_EV_ROT 또는 FAN_EV_BTN */
    int id;     /* FAN_ENCODER_ID 또는 FAN_BUTTON_ID */
    int value;  /* 회전: 1(CW), -1(CCW) / 버튼: 1 */
};

static int fan_fd = -1;
static struct fan_input_counts cached_counts;

int fan_input_open(void)
{
    if (fan_fd >= 0) {
        return fan_fd;
    }

    fan_fd = open(FAN_DEVICE_PATH, O_RDONLY);
    if (fan_fd < 0) {
        perror("fan_input_open: failed to open " FAN_DEVICE_PATH);
        return -errno;
    }

    memset(&cached_counts, 0, sizeof(cached_counts));
    return fan_fd;
}

void fan_input_close(void)
{
    if (fan_fd >= 0) {
        close(fan_fd);
        fan_fd = -1;
    }
    memset(&cached_counts, 0, sizeof(cached_counts));
}

int fan_input_fd(void)
{
    return fan_fd;
}

int fan_input_get_event(struct fan_input_event *ev)
{
    if (fan_fd < 0 || !ev) {
        errno = EINVAL;
        return -1;
    }

    /* 1. 이전에 읽어둔 배치(counts) 중 남아있는 이벤트 1개 반환 */
    if (cached_counts.cw_count > 0) {
        cached_counts.cw_count--;
        ev->type  = FAN_EV_ROT;
        ev->id    = FAN_ENCODER_ID;
        ev->value = 1; /* CW */
        return 1;
    }

    if (cached_counts.ccw_count > 0) {
        cached_counts.ccw_count--;
        ev->type  = FAN_EV_ROT;
        ev->id    = FAN_ENCODER_ID;
        ev->value = -1; /* CCW */
        return 1;
    }

    if (cached_counts.key_count > 0) {
        cached_counts.key_count--;
        ev->type  = FAN_EV_BTN;
        ev->id    = FAN_BUTTON_ID;
        ev->value = 1; /* 버튼 누름 */
        return 1;
    }

    /* 2. 캐시가 비었으면 디바이스로부터 32바이트 배치 카운트 패킷 수신 */
    struct fan_input_counts new_batch;
    ssize_t ret = read(fan_fd, &new_batch, sizeof(new_batch));

    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }

    if (ret != (ssize_t)sizeof(new_batch)) {
        errno = EIO;
        return -1;
    }

    /* ABI 버전 검증 */
    if (new_batch.abi_version != FAN_INPUT_ABI_VERSION) {
        errno = EPROTO;
        return -1;
    }

    cached_counts = new_batch;

    /* 3. 새로 채워진 배치에서 이벤트 1개 반환 */
    if (cached_counts.cw_count > 0) {
        cached_counts.cw_count--;
        ev->type  = FAN_EV_ROT;
        ev->id    = FAN_ENCODER_ID;
        ev->value = 1;
        return 1;
    }

    if (cached_counts.ccw_count > 0) {
        cached_counts.ccw_count--;
        ev->type  = FAN_EV_ROT;
        ev->id    = FAN_ENCODER_ID;
        ev->value = -1;
        return 1;
    }

    if (cached_counts.key_count > 0) {
        cached_counts.key_count--;
        ev->type  = FAN_EV_BTN;
        ev->id    = FAN_BUTTON_ID;
        ev->value = 1;
        return 1;
    }

    return 0;
}