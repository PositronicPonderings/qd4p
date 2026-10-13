/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* The few Pico SDK calls console.c uses, done with standard C on a PC.
 * Not the SDK: just enough for examples/pc/build.sh. */
#ifndef PC_PICO_STDLIB_H
#define PC_PICO_STDLIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define PICO_ERROR_TIMEOUT (-1)

/* A terminal echoes what's typed; a file doesn't, so echo its lines. */
#define ECHO_KEYS  0
#define ECHO_LINES (!isatty(0))

static inline void stdio_init_all(void) { setvbuf(stdout, NULL, _IOLBF, 0); }
static inline void sleep_ms(int ms) { (void)ms; }

static inline uint32_t time_us_32(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000000ull + (uint64_t)t.tv_nsec / 1000);
}

/* Blocks for a key (no timeout on a PC). At the end of the input: quit. */
static inline int getchar_timeout_us(uint32_t us)
{
    (void)us;
    int c = getchar();
    if (c == EOF) {
        printf("\n");
        exit(0);
    }
    return c;
}

#endif
