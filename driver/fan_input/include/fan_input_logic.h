/* SPDX-License-Identifier: GPL-2.0 */
#ifndef FAN_INPUT_LOGIC_H
#define FAN_INPUT_LOGIC_H
#include "wind_level.h"

/* Input-driver diagnostic only; this does not command the fan motor. */
#define FAN_DEBUG_MAX_LEVEL FAN_WIND_MAX_LEVEL
static inline unsigned int fan_debug_level_step(unsigned int level, int direction)
{
    return fan_wind_level_apply(level, direction, 1U);
}

/* S1 is bit 1, S2 bit 0. CW: 00 -> 01 -> 11 -> 10 -> 00.
 * Count only at a detent boundary after a complete legal sequence.
 * Adjacent contact bounce cancels; two-bit jumps discard the partial click.
 * The initial (stationary) phase defines the detent boundary. Two-transition
 * encoders also rest at the opposite phase; four-transition encoders do not.
 * Both GPIO edges must be delivered. This state belongs to one encoder and
 * must be initialized before enabling IRQs, then protected by its IRQ lock.
 */
struct fan_encoder_decoder {
    unsigned int previous;
    unsigned int detent_phase;
    unsigned int transitions_per_detent;
    int accumulator;
    int synchronized;
};

static inline int fan_encoder_steps_valid(unsigned int steps)
{
    return steps == 2U || steps == 4U;
}

static inline void fan_encoder_init(struct fan_encoder_decoder *decoder,
                                    unsigned int phase, unsigned int steps)
{
    decoder->previous = phase & 3U;
    decoder->detent_phase = phase & 3U;
    decoder->transitions_per_detent = steps;
    decoder->accumulator = 0;
    decoder->synchronized = 1;
}

static inline int fan_encoder_at_detent(const struct fan_encoder_decoder *decoder,
                                        unsigned int phase)
{
    unsigned int difference = phase ^ decoder->detent_phase;

    return difference == 0U ||
           (decoder->transitions_per_detent == 2U && difference == 3U);
}

static inline int fan_encoder_decode(struct fan_encoder_decoder *decoder,
                                     unsigned int phase, int invert)
{
    static const signed char transitions[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0
    };
    int delta, direction = 0;

    phase &= 3U;
    if (phase == decoder->previous)
        return 0;
    delta = transitions[(decoder->previous << 2) | phase];
    decoder->previous = phase;
    if (!delta) {
        decoder->accumulator = 0;
        decoder->synchronized = fan_encoder_at_detent(decoder, phase);
        return 0;
    }
    if (!decoder->synchronized) {
        if (fan_encoder_at_detent(decoder, phase))
            decoder->synchronized = 1;
        return 0;
    }
    decoder->accumulator += delta;
    if (!fan_encoder_at_detent(decoder, phase))
        return 0;
    if (decoder->accumulator == (int)decoder->transitions_per_detent)
        direction = 1;
    else if (decoder->accumulator == -(int)decoder->transitions_per_detent)
        direction = -1;
    decoder->accumulator = 0;
    return invert ? -direction : direction;
}
#endif
