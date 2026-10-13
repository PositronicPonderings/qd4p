/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    pc_hardware.c
 * @brief   The Pico's entropy sources (qd_entropy_pico.h), stood in for on a
 *          PC: the console's trng / adc / tap commands work but have no
 *          hardware to read, so they stir in the PC's own randomness.
 */
#include "qd4p.h"
#include "qd_entropy_pico.h"

static uint32_t pc_random(void *ctx)
{
    (void)ctx;
    uint32_t v;
    qd_platform_seed(&v, 1);
    return v;
}

const qd_entropy_source_t qd_entropy_trng = { "PC random (no TRNG here)", NULL, pc_random, NULL, NULL };

const qd_entropy_source_t *qd_entropy_adc(qd_entropy_adc_t *a, unsigned gpio)
{
    a->gpio = gpio;
    a->source = qd_entropy_trng;
    return &a->source;
}

void qd_entropy_event(void)
{
    qd_rng_stir32(qd_platform_micros());
}
