/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* dice: single dice without the roll engine: a standard die, a weighted
 * custom die, Fudge, percentile with a bonus die. */
#include "common.h"
#include "qd4p.h"
int main(void)
{
    start();
    qd_rng_init_hw();
    static const qd_face_t faces[] = { {1,1,""}, {2,1,""}, {3,1,""}, {4,1,""}, {5,1,""}, {6,3,""} };
    static qd_die_t loaded, d20;
    qd_die_custom(&loaded, faces, 6);
    qd_die_standard(&d20, 20);
    for (;;) {
        qd_percentile_t p;
        qd_roll_percentile(1, &p);
        printf("d20 %d loaded %d fudge %d d%% %d\n", qd_die_roll(&d20).value, qd_die_roll(&loaded).value,
               qd_die_roll(qd_fudge_die()).value, p.result);
        sleep_ms(1000);
    }
}
