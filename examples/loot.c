/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    loot.c
 * @brief   Example 5: a random table written into the app, the light way:
 *          no parser, no table format, about 13 KB of QD4P in all.
 *
 * Press a key: a 2d6 treasure, its coins rolled too.
 *
 * WHAT IT SHOWS
 *   - a table as two arrays: the ranges, and what each one gives
 *   - qd_range_find to look the roll up (no parser needed)
 *   - each entry's own dice, built in code: "3d6 x 10 gold"
 *   The example table system (examples/tables) does more (text files,
 *   links between tables, {dice} in the text) and costs the parser too:
 *   docs/SIZES.md compares the two.
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

typedef struct {
    const char *text;         /* printed with the coins' total, if it has coins */
    uint8_t     count, sides; /* the coins' dice: 3d6 ...           (0: no coins) */
    uint16_t    times;        /* ... times this: 3d6 x 10            */
} loot_t;

static const qd_range_t ranges[] = { { 2, 3 }, { 4, 6 }, { 7, 9 }, { 10, 11 }, { 12, 12 } };
static const loot_t loot[] = {
    { "a rusty key, and %ld copper pieces", 2, 6, 1 },
    { "%ld silver pieces in a torn pouch", 3, 6, 10 },
    { "%ld gold pieces", 2, 6, 10 },
    { "a silver ring, and %ld gold pieces", 4, 6, 10 },
    { "a dragon's tooth, worth more than gold", 0, 0, 0 },
};

static int32_t roll(int count, int sides)
{
    static qd_roll_t r;
    static qd_result_t res;
    qd_roll_init(&r);
    qd_roll_dice(&r, (uint16_t)count, (uint16_t)sides);
    qd_roll_run(&r, &res);
    return res.total;
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    printf("Press a key to search the body.\n");
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        int32_t v = roll(2, 6);
        const loot_t *l = &loot[qd_range_find(ranges, 5, v)];
        printf("2d6 = %2ld: ", (long)v);
        printf(l->text, (long)(l->count ? roll(l->count, l->sides) * l->times : 0));
        printf("\n");
    }
}
