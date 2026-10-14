# QD4P: QuickDice 4 Pico

Dice, cards and game results for the Raspberry Pi Pico 2, in plain C. Roll `4d6k3`, deal from a deck that knows when to reshuffle, ask "what are the odds?", and let the library work out raises, success levels and messy criticals so your program doesn't have to. Small, honest about its randomness, and commented so heavily it doubles as a textbook.

```console seed=7
> atk adv + 5 vs 15
atk adv + 5 vs 15
  1d20 adv1: 9 (8)  = 9
  total 14
  => Miss
> sw8 + 1 vs 6
sw8 + 1 vs 6
  trait:1d8 ie: 6  = 6
  wild:1d6 ie: 6 !3  = 9
  total 10
  => Success, 1 raise  (1 ace, vs 6)
> coc 65 b1 vs hard
coc 65 b1 vs hard
  1d% b1: 2 (72)  = 2
  total 2
  => Extreme success  (skill 65: hard 32, extreme 13)
> 4d6 kitten
  4d6 kitten
        ^ t needs a number: t8
```

That's the console, `qd4p_console`: flash it, open the Pico's USB serial port, and type. Every line above is something an app gets from one call.

> **Too busy to read a manual?** [Everything on one page](docs/manual/quick-reference.md).
>
> **The manual is in [`docs/manual`](docs/manual/README.md):** quick answers, seven example programs, a reference entry with a working example for every function, the complete dice notation, odds and stats, fair mode, troubleshooting, getting started, and how it all works. Every example in it is run by the test suite, so if it's written there, it works.

## What it does

| Layer | Header | What it does |
|---|---|---|
| Randomness | `qd_rng.h` | one generator, seeded from the RP2350's hardware, unbiased ranges, extra entropy stirred in from buttons and sensors, replay mode |
| Dice and decks | `qd_dice.h`, `qd_deck.h` | dice of any size; custom, Fudge and percentile dice; poker and tarot decks with reshuffle rules |
| The roll engine | `qd_roll.h` | pools, advantage, exploding, rerolls, keep and drop, success counting, buckets, maths; rerolls after the roll |
| Notation | `qd_parse.h` | text to rolls, with mistakes underlined; repeats, roll sets, mass rolls |
| Game systems | `qd_sys.h` | D&D 5e, Savage Worlds, Call of Cthulhu 7e, FATE, Vampire 5e, Dungeon Crawl Classics: their dice and their rules |
| Odds and stats | `qd_odds.h`, `qd_stats.h` | the chance of every result and outcome, exact where possible; every die tallied, with a fairness check |
| Dice in text | `qd_text.h` | `{1d4+1} wolves`, and rolls that pick from ranges: the building blocks of random tables |
| Fair mode | `qd_fair.h` | every roll numbered and fixed by a secret key, revealed at the end so players can check any roll |

**You only pay for what you use.** An app that deals cards carries 5.5 KB of QD4P; one that rolls dice built in code, 12 KB; one that reads typed rolls, 39 KB. Features you don't call cost nothing. [What QD4P costs](docs/SIZES.md) measures every one.

## Repository layout

```
CMakeLists.txt   builds the library, the console and the examples for the Pico 2
qd4p/            the library: copy this folder into your project
  qd4p.h         the one header a program includes
  qd_config.h    every limit, each one settable from your build
examples/        the console and seven example programs, each its own target qd4p_<name>
  tables/        a complete random-table system built on the library (example code)
  pc/            the console built for a PC: the fair-mode checker
tests/
  host/          tests that run on a PC, no Pico needed, the manual's examples included
  size/          what each feature costs, measured on a real Pico 2 build
tools/           size_report.py: QD4P's share of any program
docs/            the manual (docs/manual), what it costs (SIZES.md),
                 the design (design.md), the checks made on hardware
```

## Using it

Copy the `qd4p/` folder next to your `CMakeLists.txt`, then:

```cmake
add_subdirectory(qd4p)
target_link_libraries(my_app pico_stdlib qd4p)
```

```c
#include "qd4p.h"
...
qd_rng_init_hw();                                     /* once, at boot */

static qd_roll_t roll;                                /* rolls built in code: no parser needed */
static qd_result_t res;
qd_sys_dnd(&roll, QD_DND_ATTACK, 1);                  /* attack with advantage */
qd_roll_add(&roll, 5);
qd_roll_vs(&roll, 15);
qd_roll_run(&roll, &res);                             /* res.total, res.dice[] */

static qd_parsed_t typed;                             /* or rolls typed as text */
qd_parse_error_t err;
if (qd_parse("4d6k3 + 2", &typed, &err))
    qd_roll_run(&typed.roll, &res);
```

[Getting started](docs/manual/09-getting-started.md) has the whole walk-through, from an unboxed Pico to dice over USB.

## Building this repository

Pico 2 or Pico 2 W, Pico SDK 2.x. Open the folder with the Raspberry Pi Pico VS Code extension, or:

```
cmake -S . -B build -DPICO_BOARD=pico2      # or pico2_w
cmake --build build
```

One build makes every program: `build/examples/qd4p_console.uf2`, and the examples, `qd4p_hello_dice.uf2` and friends. No wiring needed: the Pico is the whole circuit.

## Tests on a PC

```
sh tests/host/run_tests.sh
```

Needs a C compiler and python3; no Pico. It tests the library, runs every example in the manual and checks what it prints, checks every roll in the notation guide, and checks every link in the documentation.

## Licence

[MIT-0](LICENSE): do what you like with it, no credit required. Published by [Positronic Ponderings](https://github.com/PositronicPonderings). How this code was made: [AI_DISCLOSURE.md](AI_DISCLOSURE.md).
