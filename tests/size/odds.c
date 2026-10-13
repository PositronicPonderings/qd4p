/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* odds: the chance of each result of 4d6 keep 3 (exact) and of 2d6 ie
 * (simulated), built in code. */
#include "common.h"
#include "qd4p.h"
int main(void)
{
    start();
    qd_rng_init_hw();
    static qd_roll_t a, b;
    static qd_odds_t o;
    qd_roll_init(&a); qd_pool_keep(qd_roll_dice(&a, 4, 6), QD_KEEP_HIGH, 3);
    qd_roll_init(&b); qd_pool_explode(qd_roll_dice(&b, 2, 6), qd_test_max(), true);
    for (;;) {
        qd_odds(&a, &o); printf("4d6k3 average %.2f, 18 or more %.2f%%\n", o.mean, 100 * qd_odds_at_least(&o, 18));
        qd_odds(&b, &o); printf("2d6 ie average %.2f\n", o.mean);
        sleep_ms(1000);
    }
}
