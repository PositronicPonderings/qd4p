/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* deck: poker, the Savage Worlds action deck and tarot, dealt and named. */
#include "common.h"
#include "qd4p.h"
static void deal(qd_deck_t *d, int n)
{
    qd_card_draw_t hand[5];
    char name[32];
    int got = qd_deck_deal(d, hand, n);
    for (int i = 0; i < got; i++) printf(" %s", qd_card_name(hand[i].card, name, sizeof(name)));
    printf("\n");
}
int main(void)
{
    start();
    qd_rng_init_hw();
    static qd_deck_t poker, sw, tarot;
    qd_deck_poker(&poker, 0);
    qd_deck_poker(&sw, 2);
    sw.policy = QD_RESHUFFLE_AFTER_TRIGGER | QD_RESHUFFLE_WHEN_EMPTY;
    qd_deck_tarot(&tarot);
    for (;;) { deal(&poker, 5); deal(&sw, 5); deal(&tarot, 3); sleep_ms(1000); }
}
