/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_entropy_host.c
 * @brief   Platform hooks for PC builds (tests and PC tools only; the Pico
 *          build uses qd_entropy_pico.c instead).
 *
 * The seed here is the clock and an address: different each run, but
 * guessable. That's fine on a PC, where it only matters for tests that
 * aren't in replay mode. Never use this file on the device.
 */
#include <time.h>

#include "qd_rng.h"

void qd_platform_seed(uint32_t *out, size_t words)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    uint32_t local;
    uint32_t base = (uint32_t)ts.tv_nsec ^ (uint32_t)ts.tv_sec ^ (uint32_t)(uintptr_t)&local;
    for (size_t i = 0; i < words; i++)
        out[i] = base * 0x9E3779B9u + (uint32_t)i * 0x85EBCA6Bu;
}

uint32_t qd_platform_micros(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (uint32_t)(ts.tv_sec * 1000000u + ts.tv_nsec / 1000);
}
