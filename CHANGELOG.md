# Changelog

## QD4P 1.0.0 (2026-10-13)

The first release: everything the Dice Roller needs from its dice, built and checked milestone by milestone on a Pico 2. Ten milestones, one library, and a manual in which every example runs.

### The library (`qd4p/`)

- **Randomness** (`qd_rng.h`): one xoshiro128\*\* generator, seeded from the RP2350's hardware random number generator; unbiased ranges (Lemire's method); stirring in outside randomness, which can only add to it; replay mode for tests and bug reports; entropy sources for the TRNG, ADC noise and button timing (`qd_entropy_pico.h`).
- **Dice and decks** (`qd_dice.h`, `qd_deck.h`): dice of any size up to 32,767 sides; custom dice with values, weights and labels; Fudge dice; percentile dice with Call of Cthulhu bonus and penalty dice. Poker decks with 0 to 4 jokers, tarot, your own decks, with reshuffle rules (every draw, when empty, after a joker) and initiative order.
- **The roll engine** (`qd_roll.h`): pools of dice with advantage and disadvantage, exploding, imploding, rerolls, keep and drop, success counting with doubles, failures and botches, buckets, matching sets, digit dice (dd66), and maths with precedence and brackets, always applied in the same documented stages. Up to 1,000 dice a roll, with summary mode past 100. Rerolls after the roll: all of it, chosen dice, dice passing a test, better or worse of two, each keeping its history. An observer for every die rolled.
- **Dice notation** (`qd_parse.h`): text to rolls, inspired by Dice Maiden's notation; errors given with their column, in words, often with the fix. Labels, comments, several rolls on a line, repeats, roll sets that keep or drop whole rolls, and mass rolls of up to 1,000, counted. Rolls written back as tidy notation.
- **Game systems** (`qd_sys.h`): D&D 5e, Savage Worlds, Call of Cthulhu 7e, FATE, Vampire: the Masquerade 5e and Dungeon Crawl Classics, each with a builder and its rules for reading the result; outcome levels shared by them all.
- **Odds** (`qd_odds.h`): the chance of every result, of passing and of each outcome, worked out exactly by convolution or by trying every roll, by the bell curve for big sums, or simulated; never changing the rolls that follow.
- **Stats for nerds** (`qd_stats.h`): every die tallied by kind, with a chi-square fairness check.
- **Dice in text** (`qd_text.h`): `{1d4+1} wolves`, a hook for `[links]`, and rolls that pick from ranges: the building blocks of random tables.
- **Fair mode** (`qd_fair.h`): every action numbered and fixed by a secret key, whose six-word fingerprint is shown at the start and which is revealed at the end, so players can check any roll afterwards.
- **Pay for what you use**: every feature costs nothing in a program that doesn't call it. No `malloc`; every limit a setting in `qd_config.h`.

### Everything around it

- **The manual** (`docs/manual`): quick answers, seven example programs, a reference entry with a working example for every function, the complete dice notation, odds and stats, fair mode, troubleshooting, getting started, how it works, appendices, and everything on one page.
- **What it costs** (`docs/SIZES.md`): flash, RAM and UF2 size of every feature, measured on a Pico 2 build (`tests/size`), the stack each function needs, and `tools/size_report.py` for measuring your own program.
- **The console** (`examples/console.c`): everything QD4P does, one command at a time over USB serial; also built for a PC (`examples/pc`), where it checks fair-mode sessions.
- **Seven example programs** (`examples/*.c`), each its own Pico target.
- **A random-table system** (`examples/tables`) built on the library, with a text format, a PC tool and a commented C-array output, as example code to copy and change.
- **Tests on a PC** (`tests/host`): the library against an independent Python reference of the generator and fair mode, golden rolls, statistical checks, every manual example and console session run and compared, every notation example checked, every documentation link followed.

Built and checked on real hardware, ten milestones in a row. The bugs found along the way are fixed; the ones not found yet are, as always, waiting patiently.
