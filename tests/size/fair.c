/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* fair: engine.c's 4d20 in a fair-mode session: fingerprint, ten numbered
 * rolls, the key. */
#include "common.h"
#include "qd4p.h"
int main(void)
{
    start();
    qd_rng_init_hw();
    char words[QD_FAIR_TEXT];
    static qd_roll_t r;
    static qd_result_t res;
    qd_fair_start();
    qd_fair_fingerprint_words(words, sizeof(words));
    printf("%s\n", words);
    qd_roll_init(&r); qd_roll_dice(&r, 4, 20);
    for (int i = 0; i < 10; i++) {
        printf("#%lu ", (unsigned long)qd_fair_begin());
        qd_roll_run(&r, &res);
        printf("%ld\n", (long)res.total);
    }
    uint8_t key[QD_FAIR_KEY_BYTES];
    qd_fair_end(key);
    qd_fair_words(key, QD_FAIR_KEY_BYTES, words, sizeof(words));
    printf("%s\n", words);
    for (;;) sleep_ms(1000);
}
