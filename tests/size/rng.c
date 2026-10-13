/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* rng: the generator alone. A d20, a d6, and the hardware seed. */
#include "common.h"
#include "qd4p.h"
int main(void)
{
    start();
    qd_rng_init_hw();
    for (;;) { printf("d20 %ld d6 %ld\n", (long)qd_rng_between(1, 20), (long)qd_rng_between(1, 6)); sleep_ms(1000); }
}
