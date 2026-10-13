/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    odds.c
 * @brief   Example 7: "should I take the shot?" Compare two choices by
 *          their odds before rolling.
 *
 * Press a key: three choices a D&D fighter faces, with the chance of
 * each, worked out exactly.
 *
 * WHAT IT SHOWS
 *   - qd_odds: the chance of passing and of each outcome, exactly
 *   - qd_odds_at_least: the chance of enough damage
 *   - the generator isn't touched: rolling afterwards gives the same dice
 *     as if the odds had never been asked
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

static qd_parsed_t p;
static qd_odds_t   odds;

static void chance(const char *what, const char *notation)
{
    qd_parse_error_t err;
    qd_parse(notation, &p, &err);
    qd_odds(&p.roll, &odds);
    printf("  %-34s hit %5.1f%%, crit %4.1f%%\n", what, 100 * odds.pass,
           100 * odds.level[QD_LVL_CRIT + 4]);
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    printf("Press a key to weigh the options.\n");
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        printf("The ogre has AC 15 and 20 hit points left.\n");
        chance("a plain attack (+5)", "atk + 5 vs 15");
        chance("from hiding, with advantage", "atk adv + 5 vs 15");
        chance("power attack (-5 to hit)", "atk + 0 vs 15");

        qd_parse_error_t err;                    /* and the damage: does one hit do it? */
        qd_parse("2d6 + 13", &p, &err);          /* the power attack's +10 damage */
        qd_odds(&p.roll, &odds);
        printf("  power attack damage of 20 or more: %.1f%%\n", 100 * qd_odds_at_least(&odds, 20));
    }
}
