# 2. Quick answers

Find your question; follow the link. (Want every function on one page? [Too busy to read a manual](quick-reference.md).) Each goes to a reference entry (the exact syntax), an example (a working program), or both.

## Rolling dice

| How do I... | |
|---|---|
| roll a d20? | [`qd_rng_between(1, 20)`](reference/randomness.md#qd_rng_between), or [example 1](03-examples.md#1-hello-dice) |
| roll 4d6 and keep the best 3? | [`qd_roll_dice`](reference/rolls.md#qd_roll_dice) and [`qd_pool_keep`](reference/rolls.md#qd_pool_keep); [example 2](03-examples.md#2-character) |
| let the player type the roll? | [`qd_parse`](reference/parse.md#qd_parse); [example 6](03-examples.md#6-dice-box) |
| roll with advantage? | [`qd_pool_advantage`](reference/rolls.md#qd_pool_advantage), or for D&D [`qd_sys_dnd`](reference/systems.md#qd_sys_dnd) |
| make dice explode? | [`qd_pool_explode`](reference/rolls.md#qd_pool_explode) |
| count successes instead of adding? | [`qd_pool_success`](reference/rolls.md#qd_pool_success) |
| reroll 1s? | [`qd_pool_reroll`](reference/rolls.md#qd_pool_reroll) |
| reroll after the roll (a benny, a push)? | [`qd_reroll_all`](reference/rolls.md#qd_reroll_all), [`qd_reroll_dice`](reference/rolls.md#qd_reroll_dice), [`qd_reroll_matching`](reference/rolls.md#qd_reroll_matching); [example 4](03-examples.md#4-savage) |
| roll several times and keep the best rolls? | [`(7 x 4d6k3) k6`](05-notation.md#roll-sets) with [`qd_set_run`](reference/parse.md#qd_set_run) |
| roll a hundred attacks at once? | [mass rolls](reference/parse.md#qd_mass_run): `400 x atk vs 12` |
| show which dice were dropped? | [`res.dice[i].flags & QD_DIE_DROPPED`](reference/rolls.md#qd_result_t) |
| make a loaded die, or one with symbols? | [`qd_die_custom`](reference/dice.md#qd_die_custom) |
| roll d66? | [`qd_roll_digits(r, 1, "66")`](reference/rolls.md#qd_roll_digits), or `dd66` in notation |
| roll d100 with bonus dice? | [`qd_roll_percentile`](reference/dice.md#qd_roll_percentile), [`qd_sys_coc`](reference/systems.md#qd_sys_coc) |

## Game rules

| How do I... | |
|---|---|
| know if an attack hit, or crit? | [`qd_sys_dnd`](reference/systems.md#qd_sys_dnd) and [`qd_sys_outcome`](reference/systems.md#qd_sys_outcome); [example 3](03-examples.md#3-attack) |
| count raises in Savage Worlds? | [`qd_sys_sw`](reference/systems.md#qd_sys_sw) |
| get a Call of Cthulhu success level? | [`qd_sys_coc`](reference/systems.md#qd_sys_coc) |
| read a FATE roll on the ladder? | [`qd_sys_fate`](reference/systems.md#qd_sys_fate), [`qd_fate_ladder`](reference/systems.md#qd_fate_ladder) |
| handle Vampire hunger dice? | [`qd_sys_vtm`](reference/systems.md#qd_sys_vtm) |
| move a die along the DCC chain? | [`qd_sys_dcc`](reference/systems.md#qd_sys_dcc) |
| judge any roll against a target? | [`qd_roll_vs`](reference/systems.md#qd_roll_vs) |
| treat every game the same in my app? | [outcome levels](reference/systems.md#outcome-levels): above 0 is good |

## Cards and tables

| How do I... | |
|---|---|
| deal from a poker deck? a tarot deck? | [`qd_deck_poker`](reference/decks.md#qd_deck_poker), [`qd_deck_tarot`](reference/decks.md#qd_deck_tarot) |
| run Savage Worlds initiative? | [reshuffle rules](reference/decks.md#reshuffle-rules) and [`qd_card_order`](reference/decks.md#qd_card_order); [example 4](03-examples.md#4-savage) |
| make my own deck? | [`qd_deck_custom`](reference/decks.md#qd_deck_custom) |
| roll on a random table? | [`qd_range_find`](reference/text.md#qd_range_find), no parser needed: [example 5](03-examples.md#5-loot). With text files and links: [the example table system](reference/tables.md) |
| put dice in text: "{1d4+1} wolves"? | [`qd_expand`](reference/text.md#qd_expand) |

## Odds, stats, fairness

| How do I... | |
|---|---|
| show the chance of hitting? | [`qd_odds`](reference/odds.md#qd_odds); [example 7](03-examples.md#7-odds) |
| show the chance of rolling 15 or more? | [`qd_odds_at_least`](reference/odds.md#qd_odds_at_least) |
| show whether a die is behaving? | [stats for nerds](reference/stats.md), [the guide](06-odds-and-stats.md#is-my-die-cursed) |
| prove to players the rolls weren't changed? | [fair mode](07-fair-mode.md) |
| get the same rolls every time, for a test? | [`qd_rng_seed_fixed`](reference/randomness.md#qd_rng_seed_fixed) |
| make the dice even more random? | stir in button presses: [`qd_entropy_event`](reference/randomness.md#qd_entropy_event) |

## Fitting it in

| How do I... | |
|---|---|
| know what QD4P will cost my program? | [What QD4P costs](../SIZES.md); measure with [`size_report.py`](../SIZES.md#measuring-your-own-program) |
| use less RAM? | [smaller limits](../SIZES.md#making-it-smaller): `QD_MAX_ROLL_DICE=100` saves 14 KB |
| use less flash? | build rolls in code instead of parsing text: the parser is 15 KB |
| add QD4P to my own project? | [CMake in five minutes](09-getting-started.md#cmake-in-five-minutes) |
| run it on a PC? | [On a PC](09-getting-started.md#on-a-pc) |
