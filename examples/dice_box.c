/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    dice_box.c
 * @brief   Example 6: type any roll, get the dice, the total and what it
 *          means. The parser, in about sixty lines.
 *
 * Type a roll and press Enter: 4d6k3, atk adv + 5 vs 15, sw8, coc 65 b1,
 * (Fireball) 8d6. A mistake is underlined, with what's wrong.
 *
 * WHAT IT SHOWS
 *   - qd_parse: text in, a roll out, or where and why it isn't one
 *   - qd_format: the roll written back the way the parser understood it
 *   - qd_sys_outcome: the game's reading of the result
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

static qd_parsed_t p;
static qd_result_t res;

static void roll(const char *text)
{
    qd_parse_error_t err;
    if (!qd_parse(text, &p, &err)) {
        printf("  %s\n  %*s", text, err.pos, "");            /* underline the problem */
        for (int i = 0; i < err.len; i++) putchar('^');
        printf(" %s\n", err.message);
        return;
    }
    char tidy[96];
    qd_outcome_t out;
    qd_format(&p, tidy, sizeof tidy);
    qd_roll_run(&p.roll, &res);
    printf("  %s:", tidy);
    for (int i = 0; i < res.dice_count; i++)                 /* (dropped or set aside) */
        printf(res.dice[i].flags & QD_DIE_OUT ? " (%d)" : " %d", res.dice[i].value);
    if (res.flags & QD_RES_COUNTING) printf("  = %d successes\n", res.successes);
    else printf("  = %ld\n", (long)res.total);
    if (qd_sys_outcome(&p.roll, &res, &out) && out.text[0])
        printf("  %s%s%s%s\n", out.text, out.detail[0] ? " (" : "", out.detail, out.detail[0] ? ")" : "");
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    printf("Type a roll and press Enter.\n");
    char line[96];
    int n = 0;
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        if (c == '\r' || c == '\n') {
            line[n] = '\0';
            if (n) roll(line);
            n = 0;
        } else if (c >= ' ' && c < 127 && n < (int)sizeof line - 1) {
            line[n++] = (char)c;
        }
    }
}
