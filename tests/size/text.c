/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* text: dice inside text, "{1d4+1} wolves and {2d6*10} gp". */
#include "common.h"
#include "qd4p.h"
int main(void)
{
    start();
    qd_rng_init_hw();
    char out[80];
    for (;;) { qd_expand("{1d4+1} wolves guard {2d6*10} gp", out, sizeof(out), NULL); printf("%s\n", out); sleep_ms(1000); }
}
