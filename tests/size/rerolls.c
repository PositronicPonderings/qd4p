/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* rerolls: engine.c's 4d6 keep 3, then every kind of reroll after it. */
#include "common.h"
#include "qd4p.h"
int main(void)
{
    start();
    qd_rng_init_hw();
    static qd_roll_t r;
    static qd_result_t res;
    qd_roll_init(&r);
    qd_pool_keep(qd_roll_dice(&r, 4, 6), QD_KEEP_HIGH, 3);
    const uint16_t two[] = { 0, 1 };
    for (;;) {
        qd_roll_run(&r, &res);              printf("%ld", (long)res.total);
        qd_reroll_dice(&r, &res, two, 2);   printf(" %ld", (long)res.total);
        qd_reroll_matching(&r, &res, -1, qd_test_le(2)); printf(" %ld", (long)res.total);
        qd_reroll_keep(&r, &res, true);     printf(" %ld", (long)res.total);
        qd_reroll_all(&r, &res);            printf(" %ld\n", (long)res.total);
        sleep_ms(1000);
    }
}
