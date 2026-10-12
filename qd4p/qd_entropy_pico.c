/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_entropy_pico.c
 * @brief   Pico 2 platform hooks and entropy sources. The only QD4P file
 *          that needs the Pico SDK.
 *
 * The hardware seed comes from the SDK's pico_rand, which on the RP2350
 * draws on the chip's TRNG (true random number generator) along with other
 * on-chip noise.
 */
#include "pico/stdlib.h"
#include "pico/rand.h"
#include "hardware/adc.h"

#include "qd_entropy_pico.h"

/* --- Platform hooks ------------------------------------------------------ */
void qd_platform_seed(uint32_t *out, size_t words)
{
    for (size_t i = 0; i < words; i++)
        out[i] = get_rand_32();
}

uint32_t qd_platform_micros(void)
{
    return time_us_32();
}

/* --- TRNG ---------------------------------------------------------------- */
static uint32_t trng_sample(void *ctx)
{
    (void)ctx;
    return get_rand_32();
}

const qd_entropy_source_t qd_entropy_trng = { "Hardware TRNG", NULL, trng_sample, NULL, NULL };

/* --- ADC noise ----------------------------------------------------------- */
static bool adc_begin(void *ctx)
{
    const qd_entropy_adc_t *a = (const qd_entropy_adc_t *)ctx;
    if (a->gpio < 26 || a->gpio > 29)
        return false;
    adc_init();
    adc_gpio_init(a->gpio);           /* analog input, digital buffer off (rule 1) */
    adc_select_input(a->gpio - 26);
    return true;
}

static uint32_t adc_sample(void *ctx)
{
    (void)ctx;
    /* Four readings packed together: mostly the same high bits, but the low
     * bits wander with electrical noise. Mixing does the rest. */
    uint32_t v = 0;
    for (int i = 0; i < 4; i++)
        v = (v << 8) ^ adc_read();
    return v;
}

const qd_entropy_source_t *qd_entropy_adc(qd_entropy_adc_t *a, unsigned gpio)
{
    a->gpio = gpio;
    a->source.name   = "ADC noise";
    a->source.begin  = adc_begin;
    a->source.sample = adc_sample;
    a->source.end    = NULL;
    a->source.ctx    = a;
    return &a->source;
}

/* --- Human timing ---------------------------------------------------------- */
void qd_entropy_event(void)
{
    uint64_t t = time_us_64();
    qd_rng_stir(&t, sizeof(t));
}
