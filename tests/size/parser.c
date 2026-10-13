/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* parser: the same five rolls as systems.c, typed as notation. */
#include "common.h"
#include "qd4p.h"
int main(void)
{
    start();
    qd_rng_init_hw();
    static const char *const rolls[] = { "4d20", "sw8", "2d6 ie", "atk adv vs 10", "coc 50 v hard" };
    static qd_parsed_t p;
    static qd_result_t res;
    for (;;) {
        for (int i = 0; i < 5; i++) {
            qd_parse_error_t err;
            qd_outcome_t o;
            if (!qd_parse(rolls[i], &p, &err)) { printf("%s\n", err.message); continue; }
            qd_roll_run(&p.roll, &res);
            qd_sys_outcome(&p.roll, &res, &o);
            printf("%s: %ld %s\n", rolls[i], (long)res.total, o.text);
        }
        sleep_ms(1000);
    }
}
