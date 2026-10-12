/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FAN_EVENT_QUEUE_H
#define FAN_EVENT_QUEUE_H
#include <linux/types.h>
#include "fan_ioctl.h"
int fan_event_device_init(void);
void fan_event_device_start(void);
void fan_event_device_exit(void);
/* Caller holds its encoder lock; returns whether the shared reader needs wakeup. */
bool fan_event_enqueue(__u8 type, __u8 id, __s8 value);
void fan_event_wake(void);
#endif
