/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* systems: the five rolls of the request, built in code, judged by their
 * game: 4d20, sw8, 2d6 ie, atk adv vs 10, coc 50 vs hard. */
#include "common.h"
#include "qd4p.h"
static void show(const char *what, const qd_roll_t *r)
{
    static qd_result_t res;
    qd_outcome_t o;
    qd_roll_run(r, &res);
    qd_sys_outcome(r, &res, &o);
    printf("%s: %ld %s %s\n", what, (long)res.total, o.text, o.detail);
}
int main(void)
{
    start();
    qd_rng_init_hw();
    static qd_roll_t r;
    for (;;) {
        qd_roll_init(&r); qd_roll_dice(&r, 4, 20);                                       show("4d20", &r);
        qd_sys_sw(&r, 8, true);                                                         show("sw8", &r);
        qd_roll_init(&r); qd_pool_explode(qd_roll_dice(&r, 2, 6), qd_test_max(), true); show("2d6 ie", &r);
        qd_sys_dnd(&r, QD_DND_ATTACK, 1); qd_roll_vs(&r, 10);                            show("atk adv vs 10", &r);
        qd_sys_coc(&r, 50, 0); qd_roll_vs(&r, QD_COC_HARD);                              show("coc 50 vs hard", &r);
        sleep_ms(1000);
    }
}
