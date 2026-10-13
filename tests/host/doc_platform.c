/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    doc_platform.c
 * @brief   The platform hooks for the manual's examples: "hardware"
 *          randomness that is the same every run.
 *
 * Every example in docs/manual is compiled and run by doc_examples.py, and
 * what it prints is checked against the output shown beneath it in the
 * manual. That only works if the dice come up the same every time, so
 * here the "hardware random source" is a fixed sequence and the clock
 * ticks by a steady millisecond. On a Pico, qd_entropy_pico.c reads the
 * real TRNG and every run is different, as it should be.
 */
#include "qd_rng.h"

void qd_platform_seed(uint32_t *out, size_t words)
{
    static uint32_t n = 2026;
    for (size_t i = 0; i < words; i++) {
        uint32_t z = (n += 0x9E3779B9u);
        z = (z ^ (z >> 16)) * 0x85EBCA6Bu;
        z = (z ^ (z >> 13)) * 0xC2B2AE35u;
        out[i] = z ^ (z >> 16);
    }
}

uint32_t qd_platform_micros(void)
{
    static uint32_t t;
    return t += 1000;
}
