# Appendices

[A. Error codes](#error-codes) · [B. Warnings and flags](#warnings-and-flags) · [C. Settings](#settings) · [D. What it costs](#what-it-costs) · [E. Glossary](#glossary)

---

## Error codes

Functions that build or run a roll return `qd_err_t` (in `qd_roll.h`); [`qd_err_text`](reference/rolls.md#qd_err_t) turns any of them into words.

| Code | Value | Means | Typically from |
|---|---|---|---|
| `QD_OK` | 0 | it worked | |
| `QD_ERR_FULL` | 1 | a builder ran out of room: pools, steps, buckets, the sum | `qd_roll_run` after `qd_roll_dice` returned `NULL` |
| `QD_ERR_NO_DICE` | 2 | a pool of 0 dice, or a 0-sided die | `qd_roll_run` |
| `QD_ERR_TOO_MANY_DICE` | 3 | more than `QD_MAX_ROLL_DICE` dice before exploding | `qd_roll_run` |
| `QD_ERR_DIGITS` | 4 | bad digit dice, or exploding or rerolling them | `qd_roll_digits`, `qd_roll_run` |
| `QD_ERR_FOREVER` | 5 | an explode-forever or reroll-forever that every face passes | `qd_roll_run` |
| `QD_ERR_SUM` | 6 | the sum doesn't leave exactly one number | low-level building with `qd_roll_push` |
| `QD_ERR_DIV_ZERO` | 7 | divide by the number 0 | `qd_roll_div(r, 0)` |
| `QD_ERR_PERCENT` | 8 | percentile dice with steps or advantage, or more than 3 bonus dice | `qd_roll_run` |
| `QD_ERR_REROLL` | 9 | that die can't be rerolled | `qd_reroll_dice`, `qd_reroll_matching` |

Notation that won't parse isn't an error code: [`qd_parse`](reference/parse.md#qd_parse) returns `false` and fills in a [`qd_parse_error_t`](reference/parse.md#qd_parse_error_t) with the column and the problem in words. Every message: [the notation guide](05-notation.md#errors).

---

## Warnings and flags

**Warnings** (`res.warnings`, [`qd_roll_warnings`](reference/rolls.md#qd_roll_warnings)): the roll ran, but it's probably not what was meant.

| Warning | Means |
|---|---|
| `QD_WARN_ONE_SIDED` | a d1, or a 1 in digit dice: it always shows 1 |
| `QD_WARN_KEEP_NONE` | keep 0: keeps nothing |

**Result flags** (`res.flags`): `QD_RES_SUMMARY`, `QD_RES_CAPPED`, `QD_RES_DIV_ZERO`, `QD_RES_CLAMPED`, `QD_RES_COUNTING`, `QD_RES_REROLLED`, `QD_RES_KEPT_FIRST`. **Die flags** (`res.dice[i].flags`): `QD_DIE_DROPPED`, `QD_DIE_SET_ASIDE`, `QD_DIE_REROLLED`, `QD_DIE_EXPLODED`, `QD_DIE_IMPLODED`, `QD_DIE_REPLACEMENT`, `QD_DIE_MAX`, `QD_DIE_MIN`, `QD_DIE_SUCCESS`, `QD_DIE_TWICE`, `QD_DIE_FAILURE`, `QD_DIE_BOTCH`, `QD_DIE_CAPPED`, `QD_DIE_CHANGED`. Both tables, with what each means: [results](reference/rolls.md#qd_result_t).

**Dice in text** (`info.flags`): `QD_EXPAND_CUT`, `QD_EXPAND_BAD`, `QD_EXPAND_FULL` ([the working](reference/text.md#qd_expand_info_t)).

---

## Settings

Sizes and limits, in [`qd4p/qd_config.h`](../../qd4p/qd_config.h). Change them from your `CMakeLists.txt`, on the library and marked `PUBLIC`, so the library and your program agree:

```cmake
target_compile_definitions(qd4p PUBLIC QD_MAX_ROLL_DICE=100 QD_MAX_KEPT_DICE=50)
```

| Setting | Default | Limits | Costs |
|---|---:|---|---|
| **Rolls** | | | |
| `QD_MAX_ROLL_DICE` | 1000 | dice in one roll, explosions included | 16 bytes each in the engine's working space |
| `QD_MAX_KEPT_DICE` | 100 | dice listed in a result (past it: summary mode, totals exact) | 12 bytes each in every `qd_result_t` |
| `QD_MAX_EXPLOSIONS` | 100 | extra dice one die may add by exploding | nothing |
| `QD_MAX_POOLS` | 8 | pools in one roll (`1d20 + 1d4 + 2d6` is three) | about 136 bytes each in a `qd_roll_t`, 120 in a `qd_result_t` |
| `QD_MAX_POOL_STEPS` | 4 | explode, implode and reroll steps per pool | 14 bytes each, in every pool |
| `QD_MAX_ROLL_OPS` | 24 | numbers, pools and signs in a roll's sum | 8 bytes each in a `qd_roll_t` |
| `QD_MAX_BUCKETS` | 8 | buckets per roll | 12 bytes each in a `qd_roll_t`, 2 in a `qd_result_t` |
| `QD_MAX_SETS` | 8 | matching sets reported per pool | 4 bytes each per pool, in a `qd_result_t` |
| `QD_MAX_DIGITS` | 4 | digits in digit dice | |
| `QD_MAX_NAME` | 7 | characters in a pool's group name | |
| **Notation** | | | |
| `QD_MAX_ROLL_LABEL` | 23 | characters in a label: `(Sneak attack)` | in each `qd_parsed_t` |
| `QD_MAX_COMMENT` | 47 | characters in a comment: `! flanking the ogre` | in each `qd_parsed_t` |
| `QD_MAX_REPEAT` | 20 | repeats listed one by one (`6 x 4d6k3`), and rolls in a set | 5 bytes each in a `qd_set_result_t` |
| `QD_MAX_MASS` | 1000 | repeats of a mass roll, counted not listed | nothing |
| `QD_MAX_DIE_NAME` | 15 | characters in a custom die's name: `2d{Loaded}` | 8 bytes each (one per pool) in a `qd_parsed_t` |
| `QD_MAX_NESTING` | 8 | brackets inside brackets | 72 bytes of stack per level, when used |
| **Dice in text** | | | |
| `QD_EXPAND_ROLLS` | 16 | `{rolls}` kept as the working of one `qd_expand` | 28 bytes each in a `qd_expand_info_t` |
| **Stats and odds** | | | |
| `QD_STATS_KINDS` | 16 | kinds of die tracked by stats for nerds | about 440 bytes each in a `qd_stats_t` |
| `QD_STATS_FACES` | 100 | faces counted one by one per kind | 4 bytes each per kind |
| `QD_ODDS_EXACT_RUNS` | 50000 | most rolls tried one by one for exact odds | time, not memory |
| `QD_ODDS_SAMPLES` | 10000 | rolls when simulating | time |
| `QD_ODDS_DICE_BUDGET` | 500000 | most dice rolled while simulating | time |
| `QD_ODDS_SPAN` | 1024 | widest range of totals worked out exactly | 16 bytes each in the odds tables |
| `QD_ODDS_BINS` | 256 | entries in an odds distribution | about 8 bytes each, in `qd_odds_t` and the odds tallies |
| **Dice and decks** | | | |
| `QD_MAX_FACES` | 30 | faces on a custom die | 8 bytes each in every `qd_die_t` |
| `QD_MAX_CARDS` | 255 | cards in one deck | 1 bit each in a `qd_deck_t` |
| `QD_MAX_LABEL` | 4 | characters in a face or card label | in every face and card |

The example table system has its own, in `examples/tables/table.h`: `TBL_DEPTH` (4: links within links, 216 bytes of stack each), `TBL_TEXT` (320 characters of result) and `TBL_STEPS` (24 rolls of working).

---

## What it costs

Flash and RAM, measured on a Pico 2 build; [`docs/SIZES.md`](../SIZES.md) has every feature and every buffer.

| A program that... | Flash added | QD4P's RAM |
|---|---:|---:|
| rolls dice with the generator alone | 2.0 KB | 26 B |
| deals cards | 5.5 KB | 26 B |
| builds rolls in code and runs them | 12.4 KB | 15.7 KB |
| ... and judges them by game rules | 17.9 KB | 15.7 KB |
| reads notation (the parser) | 39.2 KB | 15.9 KB |
| works out odds | 34.5 KB | 44.0 KB |
| uses fair mode | 5.3 KB more | 22 B more |

With `QD_MAX_ROLL_DICE=100` and `QD_MAX_KEPT_DICE=50`, the 15.7 KB of RAM is 1.6 KB. Measure your own program with [`tools/size_report.py`](../../tools/size_report.py).

---

## Glossary

**Ace.** Savage Worlds' word for a die that explodes.

**Action** (fair mode). One roll, reroll, deal or table roll, with its number.

**Advantage, disadvantage.** Roll one more die than needed and set aside the lowest (advantage) or the highest (disadvantage). D&D 5e's advantage on a d20 is `1d20 adv`.

**Botch.** A die counted as a disaster, on its own, without changing the successes: `b1` counts 1s.

**Bucket.** A count of the dice that pass a test: `[<3, =6]` counts the low dice and the sixes.

**Builder.** A function that sets up a roll in code: [`qd_roll_dice`](reference/rolls.md#qd_roll_dice), [`qd_sys_sw`](reference/systems.md#qd_sys_sw).

**Chi-square.** The fairness check's measure of how far a die's face counts are from a fair die's. [How it works](10-how-it-works.md#the-fairness-check).

**Convolution.** Combining two dice's chances into the chances of their sum; how exact odds of sums are worked out.

**Custom die.** A die with faces of your choosing: values, weights, labels. `d{name}` in notation.

**Digit dice.** Dice read as the digits of one number: `dd66` is a tens d6 and a units d6, 11 to 66.

**Dropped.** A die rolled but not kept, by keep or drop: shown in brackets.

**Entropy.** Unpredictability. Stirring adds it to the generator.

**Explode.** A die that passes a test (usually its highest face) rolls one more die, which joins the pool. `ie`: again and again.

**Fingerprint** (fair mode). Six words worked out from the session's secret key, shown at the start: they lock the key in without revealing it.

**Fudge die.** A die with faces -1, -1, 0, 0, +1, +1, shown as `[-]`, `[ ]` and `[+]`. FATE uses four: `4dF`.

**Group name.** A name for a pool, before its dice: `hunger:2d10`.

**Implode.** A die that passes a test rolls one more die, which is subtracted.

**Keep, drop.** Keep only some of a pool's dice: `k3` keeps the 3 highest, `d1` drops the lowest.

**Label** (notation). A name for a roll, in brackets at the start: `(Bite) 1d20 + 5`.

**Lemire's method.** The way QD4P turns random bits into a fair die roll without the bias of `%`. [How it works](10-how-it-works.md#unbiased-ranges).

**Level** (outcomes). How good a result is in its game: higher is better, below 0 a failure. [Outcome levels](reference/systems.md#outcome-levels).

**Mass roll.** Many repeats of a roll, counted rather than listed: `400 x atk vs 12`.

**Natural.** The die as it came up, before any modifier: a natural 20.

**Outcome.** What a result means in its game: "Hard success", "2 raises".

**Pool.** Dice of one kind and what to do with them, inside a roll: `4d6 k3` is one pool, `1d20 + 1d4` two.

**Raise.** Savage Worlds: every 4 points over the target.

**Replay mode.** The generator started from a fixed seed: the same rolls every time, on any machine.

**Reroll** (modifier). Dice that pass a test are rolled again as they're rolled: `r1`. **After the roll**: rerolling something already rolled, for a benny or a push.

**Roll.** One or more pools and a sum, ready to run: a `qd_roll_t`.

**Roll set.** Several rolls of the same thing, kept or dropped as whole rolls: `(7 x 4d6k3) k6`.

**Set aside.** A die discarded by advantage or disadvantage, before anything explodes.

**Shift** (FATE). The difference between a roll and its opposition.

**Stirring.** Mixing new randomness into the generator. It can only add unpredictability.

**Success counting.** Counting dice that pass a test instead of adding them up: `6d10 t8`.

**Summary mode.** A result of more than 100 dice: the rest are counted, not listed; every total stays exact.

**Target.** What a roll must reach to pass: `vs 15`.

**Trigger card.** A card whose appearance makes the next deal start with a reshuffle: the jokers in the Savage Worlds action deck.

**TRNG.** True random number generator: the RP2350's circuit that measures electrical noise.

**xoshiro128\*\*.** The generator: fast, small, and statistically sound. [How it works](10-how-it-works.md#the-generator).
