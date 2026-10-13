/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/* tables_own: two 12-entry tables written into the program as arrays,
 * dice built in code, qd_range_find to look the roll up. No parser. */
#include "common.h"
#include "qd4p.h"
static const qd_range_t monster_ranges[11] = { {2,2},{3,3},{4,4},{5,5},{6,6},{7,7},{8,8},{9,9},{10,10},{11,11},{12,12} };
static const char *const monsters[11] = { "A dragon, asleep on a hoard", "ghouls, hungry", "A troll under the bridge",
    "goblins with spears", "A lost merchant and his mule", "Nothing but wind in the trees", "wolves, circling",
    "A wandering knight seeking a quest", "ogres arguing over a sheep", "A hermit who knows the way",
    "A wyvern, diving from above" };
static const char *const inns[12] = { "The Prancing Pony", "The Green Dragon", "The Rusty Anchor", "The Sleeping Giant",
    "The Golden Goose", "The Broken Wheel", "The Laughing Fox", "The Silver Stag", "The Drunken Dwarf",
    "The Crooked Crown", "The Red Lantern", "The Wandering Bard" };
static int32_t roll(int n, int s, int add)
{
    static qd_roll_t r;
    static qd_result_t res;
    qd_roll_init(&r); qd_roll_dice(&r, (uint16_t)n, (uint16_t)s);
    if (add) qd_roll_add(&r, add);
    qd_roll_run(&r, &res);
    return res.total;
}
int main(void)
{
    start();
    qd_rng_init_hw();
    for (;;) {
        int k = qd_range_find(monster_ranges, 11, roll(2, 6, 0));
        if (k == 1) printf("%ld %s\n", (long)roll(1, 4, 1), monsters[k]); else printf("%s\n", monsters[k]);
        printf("%s\n", inns[roll(1, 12, 0) - 1]);
        sleep_ms(1000);
    }
}
