# 3. Examples

Seven small programs in [`examples/`](../../examples), from "press a key, roll a d20" to a dice box that reads any roll you type. Each is a complete Pico 2 program that talks over USB serial: one build makes them all (`build/examples/qd4p_<name>.uf2`), so trying another means loading another file. Open the Pico's serial port (VS Code's Serial Monitor, or `screen /dev/ttyACM0 115200`) and press keys.

Each is shown whole, followed by what it printed for some keys. The test suite runs every one on a PC with the same keys and checks it prints exactly that; your dice on a Pico will differ.

| | Program | Shows | Uses the parser? |
|---|---|---|---|
| 1 | [hello_dice](#1-hello-dice) | the smallest dice roller | no |
| 2 | [character](#2-character) | building a roll once and running it often; reading the dice | no |
| 3 | [attack](#3-attack) | D&D 5e: advantage, critical hits, damage | no |
| 4 | [savage](#4-savage) | Savage Worlds: the action deck, raises, bennies | no |
| 5 | [loot](#5-loot) | a random table in the app, the light way | no |
| 6 | [dice_box](#6-dice-box) | any roll typed in; errors underlined | yes |
| 7 | [odds](#7-odds) | choosing by the odds | yes |

The console, `qd4p_console`, is the eighth: everything QD4P does, one command at a time. [Getting started](09-getting-started.md#a-tour-of-the-console) tours it.

---

## 1. Hello dice

`qd4p_hello_dice` · [`hello_dice.c`](../../examples/hello_dice.c)

The smallest dice roller. Three lines of it are QD4P: seed the generator from the hardware at boot, stir in the timing of each key, roll. Everything else is the Pico SDK's USB serial.

```c example=ex_hello file=examples/hello_dice.c input="xxxxx"
/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    hello_dice.c
 * @brief   Example 1: the smallest dice roller. Press a key, roll a d20.
 *
 * Open the Pico's USB serial port, press any key: a d20. Three lines of it
 * are QD4P: seed the generator at boot, stir in each key press, roll.
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();                         /* seed from the RP2350's TRNG */
    printf("Press a key to roll a d20.\n");
    for (;;) {
        int c = getchar_timeout_us(100000);   /* wait up to 0.1 s for a key */
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();                   /* the key's timing: free randomness */
        long d20 = (long)qd_rng_between(1, 20);
        printf("d20: %2ld%s\n", d20, d20 == 20 ? "  natural 20!" : d20 == 1 ? "  ouch" : "");
    }
}
```

```text output
Press a key to roll a d20.
d20:  6
d20:  4
d20: 19
d20: 15
d20: 12
```

---

## 2. Character

`qd4p_character` · [`character.c`](../../examples/character.c)

D&D ability scores: seven rolls of 4d6 keep the best 3, the worst roll dropped. The roll is built once, `4d6 k3`, and run seven times: a roll never changes when it runs, so build it once and run it every time the button is pressed. Each die is shown, the dropped one in brackets.

```c example=ex_character file=examples/character.c input="x"
/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    character.c
 * @brief   Example 2: D&D ability scores, built in code (no parser).
 *
 * Press a key for a new character: seven rolls of 4d6 keep the best 3,
 * the worst of the seven dropped, each shown with its dice and modifier.
 *
 * WHAT IT SHOWS
 *   - building a roll once (4d6, keep the highest 3) and running it again
 *     and again: every run is a fresh roll
 *   - reading a result: each die's value, and whether it was dropped
 *   - no parser: rolls built in code keep the program about 15 KB smaller
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

#define ROLLS 7                               /* roll seven, keep the best six */

static qd_roll_t   stat;                      /* static: a qd_result_t is 2 KB, */
static qd_result_t res[ROLLS];                /* too big for the Pico's 2 KB stack */

static void new_character(void)
{
    int worst = 0;
    for (int i = 0; i < ROLLS; i++) {
        qd_roll_run(&stat, &res[i]);
        if (res[i].total < res[worst].total) worst = i;   /* ties: the first stays worst */
    }
    int sum = 0;
    for (int i = 0; i < ROLLS; i++) {
        printf("  %2ld ", (long)res[i].total);
        for (int d = 0; d < res[i].dice_count; d++)               /* the dice: (dropped) */
            printf(res[i].dice[d].flags & QD_DIE_DROPPED ? " (%d)" : "  %d ", res[i].dice[d].value);
        if (i == worst) {
            printf("   dropped\n");
        } else {
            /* the 5e modifier: (score - 10) / 2, rounded down: 8-9 is -1,
             * 10-11 is +0, 12-13 is +1. C rounds toward zero, so the
             * scores under 10 take the long way round. */
            int score = res[i].total;
            int mod = score >= 10 ? (score - 10) / 2 : -((11 - score) / 2);
            printf("   %+d\n", mod);
            sum += res[i].total;
        }
    }
    printf("  total %d\n\n", sum);
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();

    qd_roll_init(&stat);                                          /* 4d6, keep 3 */
    qd_pool_keep(qd_roll_dice(&stat, 4, 6), QD_KEEP_HIGH, 3);

    printf("Press a key for a new character.\n");
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        new_character();
    }
}
```

```text output
Press a key for a new character.
  13  (2)  6   3   4    +1
  10   5  (2)  2   3    +0
  10   4   4  (2)  2    +0
   9  (1)  6   1   2    dropped
  16   6  (1)  5   5    +3
  11   3   4   4  (2)   +0
  14   4   5  (2)  5    +2
  total 74
```

The results are `static` for a reason: seven `qd_result_t` are 15 KB, and the Pico's stack is 2 KB. A local variable that size would quietly run off the end of it.

---

## 3. Attack

`qd4p_attack` · [`attack.c`](../../examples/attack.c)

A D&D 5e attack and its damage. [`qd_sys_dnd`](reference/systems.md#qd_sys_dnd) builds the d20 (with advantage or disadvantage), [`qd_sys_outcome`](reference/systems.md#qd_sys_outcome) reads it the 5e way, and the outcome decides the next roll: a hit rolls 1d8 + 3, a critical hit doubles the dice. The keys here: two attacks, advantage and one more, disadvantage, then neither and one more.

```c example=ex_attack file=examples/attack.c input="  a  d n "
/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    attack.c
 * @brief   Example 3: a D&D 5e attack, then its damage, critical hits
 *          doubled. Game systems built in code (no parser).
 *
 * Keys:  a  advantage   d  disadvantage   n  neither   space  attack
 *
 * An attack is +5 to hit against AC 15. A hit rolls 1d8 + 3 damage; a
 * critical hit rolls the damage dice twice (2d8 + 3), as the 5e rules say.
 *
 * WHAT IT SHOWS
 *   - qd_sys_dnd builds the attack (the d20, with advantage or not) and
 *     qd_sys_outcome reads it: Hit, Miss, Critical hit
 *   - choosing the next roll from the last one's outcome
 *   - qd_roll_t is small to rebuild: an app can build each roll fresh
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

static qd_roll_t   roll;
static qd_result_t res;

static void attack(int adv)
{
    qd_outcome_t out;
    qd_sys_dnd(&roll, QD_DND_ATTACK, adv);       /* the d20, advantage or not */
    qd_roll_add(&roll, 5);                       /* +5 to hit */
    qd_roll_vs(&roll, 15);                       /* against AC 15 */
    qd_roll_run(&roll, &res);
    qd_sys_outcome(&roll, &res, &out);

    printf("attack%s: d20", adv > 0 ? " (advantage)" : adv < 0 ? " (disadvantage)" : "");
    for (int i = 0; i < res.dice_count; i++)
        printf(res.dice[i].flags & QD_DIE_SET_ASIDE ? " (%d)" : " %d", res.dice[i].value);
    printf(" + 5 = %ld: %s\n", (long)res.total, out.text);
    if (!out.pass)
        return;

    qd_roll_init(&roll);                         /* damage: 1d8 + 3, dice doubled on a crit */
    qd_roll_dice(&roll, out.critical ? 2 : 1, 8);
    qd_roll_add(&roll, 3);
    qd_roll_run(&roll, &res);
    printf("  damage:");
    for (int i = 0; i < res.dice_count; i++) printf(" %d", res.dice[i].value);
    printf(" + 3 = %ld%s\n", (long)res.total, out.critical ? "  (critical: dice doubled)" : "");
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    int adv = 0;
    printf("+5 to hit against AC 15, 1d8+3 damage.\n");
    printf("a: advantage  d: disadvantage  n: neither  space: attack\n");
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        if (c == 'a') adv = 1;
        else if (c == 'd') adv = -1;
        else if (c == 'n') adv = 0;
        else if (c == ' ') attack(adv);
    }
}
```

```text output
+5 to hit against AC 15, 1d8+3 damage.
a: advantage  d: disadvantage  n: neither  space: attack
attack: d20 6 + 5 = 11: Miss
attack: d20 4 + 5 = 9: Miss
attack (advantage): d20 (10) 14 + 5 = 19: Hit
  damage: 1 + 3 = 4
attack (advantage): d20 (8) 11 + 5 = 16: Hit
  damage: 8 + 3 = 11
attack (disadvantage): d20 2 (6) + 5 = 7: Miss
attack: d20 15 + 5 = 20: Hit
  damage: 4 + 3 = 7
```

---

## 4. Savage

`qd4p_savage` · [`savage.c`](../../examples/savage.c)

Savage Worlds at the table: the action deck deals initiative (two jokers; the deck reshuffles after a joker), a Fighting roll against Parry 6 reads its raises and aces, and a benny rerolls it, keeping the history.

```c example=ex_savage file=examples/savage.c input="ifbbif"
/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    savage.c
 * @brief   Example 4: Savage Worlds. Initiative from the action deck, a
 *          Fighting roll with raises, and a benny to reroll it.
 *
 * Keys:  i  deal initiative   f  Fighting (d8, against Parry 6)   b  spend a benny
 *
 * WHAT IT SHOWS
 *   - the action deck: a poker deck with two jokers that reshuffles after
 *     a joker comes up, and qd_card_order for who goes first
 *   - qd_sys_sw: a trait die and a wild die, both acing, the better one
 *     counting; raises, aces and snake eyes read by qd_sys_outcome
 *   - qd_reroll_all: a benny rerolls the whole roll and keeps the history
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

static const char *const party[] = { "Ana", "Bors", "Cleo", "Ogre" };
#define PARTY 4

static qd_deck_t   deck;
static qd_roll_t   fighting;
static qd_result_t res;
static bool        rolled;

static void initiative(void)
{
    qd_card_draw_t hand[PARTY];
    char name[24];
    if (deck.triggered) printf("(a joker came up last round: reshuffled)\n");
    qd_deck_deal(&deck, hand, PARTY);            /* one card each, one deal */
    for (int turn = 0; turn < PARTY; turn++) {   /* highest card acts first */
        int best = -1;
        for (int i = 0; i < PARTY; i++)
            if (hand[i].card && (best < 0 || qd_card_order(hand[i].card) > qd_card_order(hand[best].card)))
                best = i;
        printf("  %-5s %s\n", party[best], qd_card_name(hand[best].card, name, sizeof name));
        hand[best].card = NULL;
    }
}

static void show(const char *what)
{
    qd_outcome_t out;
    qd_sys_outcome(&fighting, &res, &out);
    printf("%s: trait %ld, wild %ld: %s (%s)\n", what, (long)res.pools[0].total,
           (long)res.pools[1].total, out.text, out.detail);
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    qd_deck_poker(&deck, 2);
    deck.policy = QD_RESHUFFLE_AFTER_TRIGGER | QD_RESHUFFLE_WHEN_EMPTY;
    qd_sys_sw(&fighting, 8, true);               /* Fighting d8, a Wild Card */
    qd_roll_vs(&fighting, 6);                    /* against Parry 6 */

    printf("i: initiative  f: Fighting d8 vs Parry 6  b: benny\n");
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        if (c == 'i') {
            initiative();
        } else if (c == 'f') {
            qd_roll_run(&fighting, &res);
            rolled = true;
            show("Fighting");
        } else if (c == 'b' && rolled) {
            qd_reroll_all(&fighting, &res);      /* keeps the old total in was_total */
            show("benny");
            printf("  (was %ld; %u benn%s spent on this roll)\n", (long)res.was_total, res.rerolls,
                   res.rerolls == 1 ? "y" : "ies");
        }
    }
}
```

```text output
i: initiative  f: Fighting d8 vs Parry 6  b: benny
  Bors  Ace of Spades
  Cleo  Ace of Diamonds
  Ogre  Ten of Hearts
  Ana   Four of Diamonds
Fighting: trait 6, wild 4: Success (vs 6)
benny: trait 13, wild 4: Success, 1 raise (1 ace, vs 6)
  (was 6; 1 benny spent on this roll)
benny: trait 21, wild 4: Success, 3 raises (2 aces, vs 6)
  (was 13; 2 bennies spent on this roll)
  Cleo  Queen of Diamonds
  Ana   Eight of Diamonds
  Ogre  Seven of Spades
  Bors  Seven of Hearts
Fighting: trait 1, wild 4: Failure (vs 6)
```

---

## 5. Loot

`qd4p_loot` · [`loot.c`](../../examples/loot.c)

A random table written into the program as two arrays, rolled with the roll engine and looked up with [`qd_range_find`](reference/text.md#qd_range_find). No parser, no file format: about 13 KB of QD4P in all. The example table system in [`examples/tables`](reference/tables.md) does more (tables as text files, links between tables, `{dice}` in the text) and brings the parser for 25 KB more; [what it costs](../SIZES.md#three-typical-programs) compares the two.

```c example=ex_loot file=examples/loot.c input="xxxxxx"
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
```

```text output
Press a key to search the body.
2d6 =  8: 70 gold pieces
2d6 =  8: 90 gold pieces
2d6 =  6: 130 silver pieces in a torn pouch
2d6 =  7: 70 gold pieces
2d6 = 10: a silver ring, and 60 gold pieces
2d6 =  5: 110 silver pieces in a torn pouch
```

---

## 6. Dice box

`qd4p_dice_box` · [`dice_box.c`](../../examples/dice_box.c)

Type any roll and press Enter. The [parser](reference/parse.md) reads it, [`qd_format`](reference/parse.md#qd_format) writes back what it understood, and the game's reading follows. A mistake is underlined, with what's wrong: that's how a player learns the notation without reading anything.

```c example=ex_dice_box file=examples/dice_box.c input="4d6k3\natk adv + 5 vs 15\nsw8\n(Fireball) 8d6\n4d6 kitten\n"
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
```

```text output
Type a roll and press Enter.
  4d6 k3: 3 (1) 5 1  = 9
  atk adv + 5 vs 15: (1) 12  = 17
  Hit
  sw8: 8 4 6 4  = 12
  Success, 2 raises (2 aces, vs 4)
  (Fireball) 8d6: 4 4 3 1 4 3 3 4  = 26
  4d6 kitten
        ^ t needs a number: t8
```

---

## 7. Odds

`qd4p_odds` · [`odds.c`](../../examples/odds.c)

Should the fighter take the shot? [`qd_odds`](reference/odds.md#qd_odds) works out each choice exactly before anything is rolled: the chance to hit, to crit, and whether a power attack's damage would finish the ogre. Asking never changes the dice that come next.

```c example=ex_odds file=examples/odds.c input="x"
/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    odds.c
 * @brief   Example 7: "should I take the shot?" Compare two choices by
 *          their odds before rolling.
 *
 * Press a key: three choices a D&D fighter faces, with the chance of
 * each, worked out exactly.
 *
 * WHAT IT SHOWS
 *   - qd_odds: the chance of passing and of each outcome, exactly
 *   - qd_odds_at_least: the chance of enough damage
 *   - the generator isn't touched: rolling afterwards gives the same dice
 *     as if the odds had never been asked
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "qd4p.h"
#include "qd_entropy_pico.h"

static qd_parsed_t p;
static qd_odds_t   odds;

static void chance(const char *what, const char *notation)
{
    qd_parse_error_t err;
    qd_parse(notation, &p, &err);
    qd_odds(&p.roll, &odds);
    printf("  %-34s hit %5.1f%%, crit %4.1f%%\n", what, 100 * odds.pass,
           100 * odds.level[QD_LVL_CRIT + 4]);
}

int main(void)
{
    stdio_init_all();
    qd_rng_init_hw();
    printf("Press a key to weigh the options.\n");
    for (;;) {
        int c = getchar_timeout_us(100000);
        if (c == PICO_ERROR_TIMEOUT)
            continue;
        qd_entropy_event();
        printf("The ogre has AC 15 and 20 hit points left.\n");
        chance("a plain attack (+5)", "atk + 5 vs 15");
        chance("from hiding, with advantage", "atk adv + 5 vs 15");
        chance("power attack (-5 to hit)", "atk + 0 vs 15");

        qd_parse_error_t err;                    /* and the damage: does one hit do it? */
        qd_parse("2d6 + 13", &p, &err);          /* the power attack's +10 damage */
        qd_odds(&p.roll, &odds);
        printf("  power attack damage of 20 or more: %.1f%%\n", 100 * qd_odds_at_least(&odds, 20));
    }
}
```

```text output
Press a key to weigh the options.
The ogre has AC 15 and 20 hit points left.
  a plain attack (+5)                hit  55.0%, crit  5.0%
  from hiding, with advantage        hit  79.8%, crit  9.8%
  power attack (-5 to hit)           hit  30.0%, crit  5.0%
  power attack damage of 20 or more: 58.3%
```
