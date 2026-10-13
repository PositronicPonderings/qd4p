/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    hello_dice.c
 * @brief   Example 1: the smallest dice roller. Press a key, roll a d20.
 *
 * Open the Pico's USB serial port, press any key: a d20. Three lines of it
 * are QD4P: seed the generator at boot, stir in each key press, roll.
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();                         /* seed from the RP2350's TRNG */
    printf("Press a key to roll a d20.\n");
    for (;;) {
        int c = getchar_timeout_us(100000);   /* wait up to 0.1 s for a key */
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();                   /* the key's timing: free randomness */
        long d20 = (long)qd_rng_between(1, 20);
        printf("d20: %2ld%s\n", d20, d20 == 20 ? "  natural 20!" : d20 == 1 ? "  ouch" : "");
    }
}
