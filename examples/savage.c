/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    savage.c
 * @brief   Example 4: Savage Worlds. Initiative from the action deck, a
 *          Fighting roll with raises, and a benny to reroll it.
 *
 * Keys:  i  deal initiative   f  Fighting (d8, against Parry 6)   b  spend a benny
 *
 * WHAT IT SHOWS
 *   - the action deck: a poker deck with two jokers that reshuffles after
 *     a joker comes up, and qd_card_order for who goes first
 *   - qd_sys_sw: a trait die and a wild die, both acing, the better one
 *     counting; raises, aces and snake eyes read by qd_sys_outcome
 *   - qd_reroll_all: a benny rerolls the whole roll and keeps the history
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

static const char *const party[] = { "Ana", "Bors", "Cleo", "Ogre" };
#define PARTY 4

static qd_deck_t   deck;
static qd_roll_t   fighting;
static qd_result_t res;
static bool        rolled;

static void initiative(void)
{
    qd_card_draw_t hand[PARTY];
    char name[24];
    if (deck.triggered) printf("(a joker came up last round: reshuffled)\n");
    qd_deck_deal(&deck, hand, PARTY);            /* one card each, one deal */
    for (int turn = 0; turn < PARTY; turn++) {   /* highest card acts first */
        int best = -1;
        for (int i = 0; i < PARTY; i++)
            if (hand[i].card && (best < 0 || qd_card_order(hand[i].card) > qd_card_order(hand[best].card)))
                best = i;
        printf("  %-5s %s\n", party[best], qd_card_name(hand[best].card, name, sizeof name));
        hand[best].card = NULL;
    }
}

static void show(const char *what)
{
    qd_outcome_t out;
    qd_sys_outcome(&fighting, &res, &out);
    printf("%s: trait %ld, wild %ld: %s (%s)\n", what, (long)res.pools[0].total,
           (long)res.pools[1].total, out.text, out.detail);
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    qd_deck_poker(&deck, 2);
    deck.policy = QD_RESHUFFLE_AFTER_TRIGGER | QD_RESHUFFLE_WHEN_EMPTY;
    qd_sys_sw(&fighting, 8, true);               /* Fighting d8, a Wild Card */
    qd_roll_vs(&fighting, 6);                    /* against Parry 6 */

    printf("i: initiative  f: Fighting d8 vs Parry 6  b: benny\n");
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        if (c == 'i') {
            initiative();
        } else if (c == 'f') {
            qd_roll_run(&fighting, &res);
            rolled = true;
            show("Fighting");
        } else if (c == 'b' && rolled) {
            qd_reroll_all(&fighting, &res);      /* keeps the old total in was_total */
            show("benny");
            printf("  (was %ld; %u benn%s spent on this roll)\n", (long)res.was_total, res.rerolls,
                   res.rerolls == 1 ? "y" : "ies");
        }
    }
}
