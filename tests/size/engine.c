/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* engine: rolls built in code and run, no game systems, no parser:
 * 4d20, 2d6 exploding forever, 4d6 keep 3, 10d10 counting 8s. */
#include "common.h"
#include "qd4p.h"
static long run(const qd_roll_t *r)
{
    static qd_result_t res;
    qd_roll_run(r, &res);
    return (res.flags & QD_RES_COUNTING) ? res.successes : (long)res.total;
}
int main(void)
{
    start();
    qd_rng_init_hw();
    static qd_roll_t a, b, c, d;
    qd_roll_init(&a); qd_roll_dice(&a, 4, 20);
    qd_roll_init(&b); qd_pool_explode(qd_roll_dice(&b, 2, 6), qd_test_max(), true);
    qd_roll_init(&c); qd_pool_keep(qd_roll_dice(&c, 4, 6), QD_KEEP_HIGH, 3);
    qd_roll_init(&d); qd_pool_success(qd_roll_dice(&d, 10, 10), qd_test_ge(8));
    for (;;) { printf("%ld %ld %ld %ld\n", run(&a), run(&b), run(&c), run(&d)); sleep_ms(1000); }
}
