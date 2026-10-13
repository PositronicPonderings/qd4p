/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* stats: engine.c's 4d20, every die counted, and the fairness check. */
#include "common.h"
#include "qd4p.h"
int main(void)
{
    start();
    qd_rng_init_hw();
    static qd_stats_t st;
    static qd_roll_t r;
    static qd_result_t res;
    qd_stats_watch(&st);
    qd_roll_init(&r); qd_roll_dice(&r, 4, 20);
    for (;;) {
        for (int i = 0; i < 100; i++) qd_roll_run(&r, &res);
        const qd_die_stats_t *d = qd_stats_find(&st, 20, NULL);
        qd_stats_report_t rep;
        if (d) { qd_stats_report(d, &rep); printf("d20: %lu rolled, %s\n", (unsigned long)d->count, rep.verdict == QD_STATS_FAIR ? "fair" : "?"); }
        sleep_ms(1000);
    }
}
