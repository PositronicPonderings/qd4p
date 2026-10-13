/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    character.c
 * @brief   Example 2: D&D ability scores, built in code (no parser).
 *
 * Press a key for a new character: seven rolls of 4d6 keep the best 3,
 * the worst of the seven dropped, each shown with its dice and modifier.
 *
 * WHAT IT SHOWS
 *   - building a roll once (4d6, keep the highest 3) and running it again
 *     and again: every run is a fresh roll
 *   - reading a result: each die's value, and whether it was dropped
 *   - no parser: rolls built in code keep the program about 15 KB smaller
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

#define ROLLS 7                               /* roll seven, keep the best six */

static qd_roll_t   stat;                      /* static: a qd_result_t is 2 KB, */
static qd_result_t res[ROLLS];                /* too big for the Pico's 2 KB stack */

static void new_character(void)
{
    int worst = 0;
    for (int i = 0; i < ROLLS; i++) {
        qd_roll_run(&stat, &res[i]);
        if (res[i].total < res[worst].total) worst = i;   /* ties: the first stays worst */
    }
    int sum = 0;
    for (int i = 0; i < ROLLS; i++) {
        printf("  %2ld ", (long)res[i].total);
        for (int d = 0; d < res[i].dice_count; d++)               /* the dice: (dropped) */
            printf(res[i].dice[d].flags & QD_DIE_DROPPED ? " (%d)" : "  %d ", res[i].dice[d].value);
        if (i == worst) {
            printf("   dropped\n");
        } else {
            /* the 5e modifier: (score - 10) / 2, rounded down: 8-9 is -1,
             * 10-11 is +0, 12-13 is +1. C rounds toward zero, so the
             * scores under 10 take the long way round. */
            int score = res[i].total;
            int mod = score >= 10 ? (score - 10) / 2 : -((11 - score) / 2);
            printf("   %+d\n", mod);
            sum += res[i].total;
        }
    }
    printf("  total %d\n\n", sum);
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();

    qd_roll_init(&stat);                                          /* 4d6, keep 3 */
    qd_pool_keep(qd_roll_dice(&stat, 4, 6), QD_KEEP_HIGH, 3);

    printf("Press a key for a new character.\n");
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        new_character();
    }
}
