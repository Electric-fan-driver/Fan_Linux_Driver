/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef FAN_INPUT_EVENT_H
#define FAN_INPUT_EVENT_H
#ifdef __KERNEL__
#include <linux/types.h>
typedef __u32 fan_u32;
typedef __u64 fan_u64;
#else
#include <stdint.h>
typedef uint32_t fan_u32;
typedef uint64_t fan_u64;
#endif
#define FAN_INPUT_ABI_VERSION 2U
#define FAN_INPUT_PACKET_SIZE 32U
/* read returns all pending counts in one batch; timestamps/order are absent.
 * ABI 2 replaces the old 24-byte event interface; rebuild the application.
 */
struct fan_input_counts {
    fan_u32 abi_version;
    fan_u32 reserved;
    fan_u64 cw_count;
    fan_u64 ccw_count;
    fan_u64 key_count;
};
#endif
