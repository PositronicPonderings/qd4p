/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    attack.c
 * @brief   Example 3: a D&D 5e attack, then its damage, critical hits
 *          doubled. Game systems built in code (no parser).
 *
 * Keys:  a  advantage   d  disadvantage   n  neither   space  attack
 *
 * An attack is +5 to hit against AC 15. A hit rolls 1d8 + 3 damage; a
 * critical hit rolls the damage dice twice (2d8 + 3), as the 5e rules say.
 *
 * WHAT IT SHOWS
 *   - qd_sys_dnd builds the attack (the d20, with advantage or not) and
 *     qd_sys_outcome reads it: Hit, Miss, Critical hit
 *   - choosing the next roll from the last one's outcome
 *   - qd_roll_t is small to rebuild: an app can build each roll fresh
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

static qd_roll_t   roll;
static qd_result_t res;

static void attack(int adv)
{
    qd_outcome_t out;
    qd_sys_dnd(&roll, QD_DND_ATTACK, adv);       /* the d20, advantage or not */
    qd_roll_add(&roll, 5);                       /* +5 to hit */
    qd_roll_vs(&roll, 15);                       /* against AC 15 */
    qd_roll_run(&roll, &res);
    qd_sys_outcome(&roll, &res, &out);

    printf("attack%s: d20", adv > 0 ? " (advantage)" : adv < 0 ? " (disadvantage)" : "");
    for (int i = 0; i < res.dice_count; i++)
        printf(res.dice[i].flags & QD_DIE_SET_ASIDE ? " (%d)" : " %d", res.dice[i].value);
    printf(" + 5 = %ld: %s\n", (long)res.total, out.text);
    if (!out.pass)
        return;

    qd_roll_init(&roll);                         /* damage: 1d8 + 3, dice doubled on a crit */
    qd_roll_dice(&roll, out.critical ? 2 : 1, 8);
    qd_roll_add(&roll, 3);
    qd_roll_run(&roll, &res);
    printf("  damage:");
    for (int i = 0; i < res.dice_count; i++) printf(" %d", res.dice[i].value);
    printf(" + 3 = %ld%s\n", (long)res.total, out.critical ? "  (critical: dice doubled)" : "");
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    int adv = 0;
    printf("+5 to hit against AC 15, 1d8+3 damage.\n");
    printf("a: advantage  d: disadvantage  n: neither  space: attack\n");
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        if (c == 'a') adv = 1;
        else if (c == 'd') adv = -1;
        else if (c == 'n') adv = 0;
        else if (c == ' ') attack(adv);
    }
}
