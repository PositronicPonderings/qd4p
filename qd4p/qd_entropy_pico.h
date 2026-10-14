/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_entropy_pico.h
 * @brief   Ready-made entropy sources for the Pico 2 (RP2350).
 *
 *     qd_rng_init_hw();                              // TRNG seed at boot
 *
 *     qd_rng_stir_source(&qd_entropy_trng, 16);      // more hardware randomness
 *
 *     qd_entropy_adc_t light;                        // a sensor on GPIO 28
 *     qd_rng_stir_source(qd_entropy_adc(&light, 28), 64);
 *
 *     // in a button or encoder handler:
 *     qd_entropy_event();                            // the moment of the click
 *
 * Not included in PC builds.
 *
 * MANUAL: docs/manual/reference/randomness.md#qd_entropy_trng, every function with an example.
 */
#ifndef QD_ENTROPY_PICO_H
#define QD_ENTROPY_PICO_H

#include "qd_rng.h"

#ifdef __cplusplus
extern "C" {
#endif

/** The RP2350's hardware random number generator. */
extern const qd_entropy_source_t qd_entropy_trng;

/** An ADC pin (GPIO 26-29). The low bits of each reading are electrical
 *  noise, whatever is wired there: a photoresistor divider, a pot, a
 *  sensor. The pin is set up as an analog input with adc_gpio_init
 *  (hardware rule 1) when sampling begins. */
typedef struct {
    qd_entropy_source_t source;
    unsigned gpio;
} qd_entropy_adc_t;

/** Fill in `a` for an ADC pin and return its source, ready for
 *  qd_rng_stir_source(). `a` must stay alive while it is used. */
const qd_entropy_source_t *qd_entropy_adc(qd_entropy_adc_t *a, unsigned gpio);

/** Call from a button press, encoder click or any human-timed event: stirs
 *  in the exact microsecond it happened. People can't time their clicks to
 *  the microsecond, so every spin of the encoder adds real randomness. */
void qd_entropy_event(void);

#ifdef __cplusplus
}
#endif

#endif /* QD_ENTROPY_PICO_H */
