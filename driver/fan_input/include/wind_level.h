/* SPDX-License-Identifier: GPL-2.0 */
#ifndef WIND_LEVEL_H
#define WIND_LEVEL_H
#include "fan_input_event.h"

#define FAN_WIND_MAX_LEVEL 4U

/* Policy helper, no global state/threads/motor commands. Caller owns level.
 * Saturation, not wraparound: CW at 4 and CCW at 0 leave level unchanged.
 * O(1) for any batch size, including UINT64_MAX; no per-count loop/overflow.
 */
static inline unsigned int fan_wind_level_apply(unsigned int level,
                                               int direction, fan_u64 count)
{
    if (level > FAN_WIND_MAX_LEVEL)
        level = FAN_WIND_MAX_LEVEL;
    if (direction > 0) {
        unsigned int room = FAN_WIND_MAX_LEVEL - level;
        return count >= room ? FAN_WIND_MAX_LEVEL : level + (unsigned int)count;
    }
    if (direction < 0)
        return count >= level ? 0U : level - (unsigned int)count;
    return level;
}
#endif
