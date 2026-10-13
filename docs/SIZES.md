# What QD4P costs

Every byte QD4P takes is a byte your program can't spend on something else: a random table, a picture, a sound. This page says what each feature costs in flash and RAM, so nothing comes as a surprise, and which settings shrink it.

The Pico 2 has 4 MB of flash (on a standard board) and 520 KB of RAM. For scale: the whole QD4P console, which uses every feature at once, is 62 KB of QD4P code and 54 KB of QD4P RAM.

- [The rule: pay for what you use](#the-rule-pay-for-what-you-use)
- [Three typical programs](#three-typical-programs)
- [Every feature, measured](#every-feature-measured)
- [What brings in what](#what-brings-in-what)
- [RAM: the library's own buffers](#ram-the-librarys-own-buffers)
- [RAM: what your program declares](#ram-what-your-program-declares)
- [Making it smaller](#making-it-smaller)
- [Tables, text and other data](#tables-text-and-other-data)
- [Measuring your own program](#measuring-your-own-program)

---

## The rule: pay for what you use

A feature you don't call costs nothing. Not "very little": nothing, or as close as makes no difference.

This works because of how the Pico SDK builds programs. Every function and every variable is compiled into its own little section (`-ffunction-sections -fdata-sections`), and the linker throws away every section nothing refers to (`--gc-sections`). So a program that deals cards but never rolls dice has the deck code in it and no roll engine; one that rolls `4d6` from code has no parser. The library is a menu, not a set meal.

QD4P is written to keep it that way: features don't reach into each other unless they need to, and buffers live with the function that uses them, so they leave with it too.

Fair mode was checked against this rule directly: a program that doesn't use it builds to exactly the same UF2 file with or without fair mode in the library. The only trace is 8 bytes of code and 4 bytes of RAM, where the generator checks whether a fair session is holding it.

## Three typical programs

What a dice-rolling app adds by using QD4P, over a program that only talks over USB (measured; the details are below):

| The app... | Flash added | QD4P's RAM | UF2 file grows by |
|---|---:|---:|---:|
| rolls dice built in code (`4d20`, `sw8`, `2d6 ie`, `atk adv vs 10`, `coc 50 vs hard`) and judges them | 18.0 KB | 15.7 KB | 34 KB |
| deals cards (poker, Savage Worlds action deck, tarot) | 5.5 KB | 26 B | 10 KB |
| draws on two 12-entry random tables, written into the app | 13.5 KB | 15.7 KB | 26 KB |
| ... the same tables through the example table system, with `{dice}` in the text | 37.7 KB | 19.7 KB | 69 KB |

Three things stand out:

- **Decks are cheap.** They use the random number generator and nothing else.
- **The parser is the big item: about 15 KB on its own, and it brings all the game systems with it.** Anything that reads notation as text needs it: `qd_parse`, dice inside text (`qd_expand`), and the example table system, which stores each table's dice as notation. An app that builds its rolls in code never pays for it.
- **The 15.7 KB of RAM is the roll engine's working space for the largest roll allowed: 1,000 dice.** An app that never rolls more than 100 dice can set that limit to 100 and get the RAM down to 1.6 KB. See [Making it smaller](#making-it-smaller).

The UF2 file is about twice the flash a program fills: each 512-byte block of a UF2 file carries 256 bytes of program. The flash figure is the one that counts against your 4 MB.

## Every feature, measured

Each program in [`tests/size`](../tests/size) uses one feature and prints its results, so the compiler can't optimise the feature away. The baseline, `base.c`, uses no QD4P at all. Each row is that program minus the baseline: what adding the feature adds to an app.

- **Flash added** is everything the feature brings: QD4P's own code, and the parts of the Pico SDK and C library it calls (the hardware random source, floating-point maths for odds).
- **QD4P's RAM** is the library's own buffers. Structures your program declares, such as a `qd_roll_t` or a `qd_result_t`, are your program's, and are listed [further down](#ram-what-your-program-declares).

<!-- measure.py: begin -->
Measured with QD4P 0.0.10, Pico SDK for `pico2`, arm-none-eabi-gcc 14.2.1, Release build. The baseline program (USB serial and `printf`, no QD4P) is 29.9 KB of flash, a 56,832-byte UF2 file.

| Program | What it uses | Flash added | of which QD4P | QD4P's RAM | UF2 added | QD4P's flash, by file |
|---|---|---:|---:|---:|---:|---|
| `rng` | the generator: `qd_rng_between` for a d20 and a d6 | 2.0 KB | 1.0 KB | 26 B | 3,584 | rng 984 B |
| `dice` | single dice: a d20, a weighted custom die, a Fudge die, d% with a bonus die | 2.8 KB | 1.7 KB | 275 B | 5,120 | rng 888 B, dice 784 B |
| `deck` | decks: poker, the Savage Worlds action deck and tarot, dealt and named | 5.5 KB | 3.4 KB | 26 B | 9,728 | deck 2.4 KB, rng 888 B |
| `engine` | the roll engine, rolls built in code: 4d20, 2d6 ie, 4d6 k3, 10d10 t8 | 12.4 KB | 10.3 KB | 15.7 KB | 24,576 | roll 8.8 KB, rng 888 B, dice 557 B |
| `systems` | the engine plus game systems, built in code and judged: 4d20, sw8, 2d6 ie, atk adv vs 10, coc 50 vs hard | 18.0 KB | 14.8 KB | 15.7 KB | 34,304 | roll 9.0 KB, sys 4.3 KB, rng 888 B, dice 557 B |
| `parser` | the same five rolls typed as notation: `qd_parse` | 39.2 KB | 32.2 KB | 15.9 KB | 70,144 | parse 15.2 KB, roll 10.5 KB, sys 5.0 KB, rng 888 B, dice 681 B |
| `rerolls` | the engine plus every kind of reroll after the roll | 14.1 KB | 12.0 KB | 18.0 KB | 28,160 | roll 10.5 KB, rng 888 B, dice 557 B |
| `odds` | the engine plus `qd_odds`: exact and simulated | 34.6 KB | 24.3 KB | 42.3 KB | 67,072 | odds 9.9 KB, roll 8.9 KB, sys 3.8 KB, rng 984 B, dice 557 B |
| `stats` | the engine plus stats for nerds and the fairness check | 16.0 KB | 12.3 KB | 15.7 KB | 31,744 | roll 8.6 KB, stats 2.2 KB, rng 888 B, dice 609 B |
| `text` | dice inside text: `qd_expand("{1d4+1} wolves ...")` (brings the parser) | 35.0 KB | 29.3 KB | 19.7 KB | 62,976 | parse 15.2 KB, roll 10.5 KB, sys 1.2 KB, text 900 B, rng 888 B, dice 681 B |
| `tables_own` | two 12-entry tables written into the app: dice built in code, `qd_range_find` | 13.5 KB | 10.2 KB | 15.7 KB | 25,600 | roll 8.6 KB, rng 888 B, dice 557 B |
| `tables_example` | the same two tables in the example table system (`examples/tables`), one with {dice} in its text | 37.7 KB | 29.5 KB | 19.7 KB | 68,608 | parse 15.2 KB, roll 10.5 KB, sys 1.2 KB, text 1016 B, rng 888 B, dice 681 B |
| `fair` | the engine plus fair mode: fingerprint, numbered rolls, the key | 19.4 KB | 15.2 KB | 15.7 KB | 34,816 | roll 8.6 KB, fair 5.0 KB, rng 1.0 KB, dice 557 B |
<!-- measure.py: end -->

The same programs with smaller limits (`QD_MAX_ROLL_DICE=100`, `QD_MAX_KEPT_DICE=50`, `QD_ODDS_SPAN=256`, `QD_ODDS_BINS=64`) need the same flash, and this much RAM:

| Program | QD4P's RAM, defaults | QD4P's RAM, smaller limits |
|---|---:|---:|
| `engine`, `systems`, `stats`, `tables_own`, `fair` | 15.7 KB | 1.6 KB |
| `parser` | 15.9 KB | 1.9 KB |
| `rerolls` | 18.0 KB | 3.3 KB |
| `text`, `tables_example` | 19.7 KB | 5.0 KB |
| `odds` | 42.3 KB | 13.0 KB |

`python3 tests/size/measure.py` rebuilds all of these on your machine; `--write` puts the first table here, and `--define NAME=VALUE` measures with other settings.

## What brings in what

Using one feature links in the ones it is built on, and only those:

| Feature | Header | Built on |
|---|---|---|
| The generator | `qd_rng.h` | nothing (plus the Pico SDK's hardware random source) |
| Single dice | `qd_dice.h` | the generator |
| Decks | `qd_deck.h` | the generator |
| The roll engine | `qd_roll.h` | the generator, single dice |
| Game systems | `qd_sys.h` | the roll engine (only the systems you build are linked; judging, `qd_sys_outcome`, links them all) |
| Rerolls | `qd_roll.h` | the roll engine |
| The parser | `qd_parse.h` | the roll engine and every game system: it understands all their words |
| Odds | `qd_odds.h` | the roll engine and the outcome code (to give the chance of each outcome) |
| Stats for nerds | `qd_stats.h` | the roll engine (as an observer: no cost to rolls when not watching) |
| Dice inside text | `qd_text.h` | the parser |
| Fair mode | `qd_fair.h` | the generator |

## RAM: the library's own buffers

QD4P never calls `malloc`. Its working space is fixed-size `static` buffers, each declared inside the file (often inside the function) that uses it, so each is linked only when that function is.

| Buffer | File | Bytes (defaults) | Sized by | Linked when the program... |
|---|---|---:|---|---|
| Working dice, sort order, sort scratch | `qd_roll.c` | 16,000 | 16 bytes x `QD_MAX_ROLL_DICE` (1000) | runs any roll (`qd_roll_run`) |
| A second result, for better-of-two | `qd_roll.c` | 2,212 | one `qd_result_t` | calls `qd_reroll_keep` |
| Dice to reroll | `qd_roll.c` | 200 | 2 bytes x `QD_MAX_KEPT_DICE` (100) | calls `qd_reroll_matching` |
| Each repeat's result | `qd_parse.c` | 2,212 each | one `qd_result_t` | calls `qd_set_run` (and another for `qd_mass_run`) |
| A parsed roll and its result | `qd_text.c` | 3,828 | `qd_parsed_t` + `qd_result_t` | calls `qd_roll_text`, `qd_expand` or `qd_pick_range` |
| Exact sums: four probability tables | `qd_odds.c` | 16,384 | 16 bytes x `QD_ODDS_SPAN` (1024) | calls `qd_odds` |
| Odds tallies | `qd_odds.c` | 2,200 | `QD_ODDS_BINS` (256) and the outcome levels | calls `qd_odds` |
| Enumeration script | `qd_odds.c` | 2,048 | 256 draws | calls `qd_odds` |
| Results while working out odds | `qd_odds.c` | 6,636 | three `qd_result_t` | calls `qd_odds` |
| The shared Fudge die | `qd_dice.c` | 248 | one `qd_die_t` | calls `qd_fudge_die` (or rolls `dF`) |
| The session key | `qd_fair.c` | 16 | | uses fair mode |
| Generator state and flags | `qd_rng.c` | 26 | | uses QD4P at all |

The example table system adds 1,808 bytes (`examples/tables/table.c`: one `qd_expand_info_t` per level of `[links]`, `TBL_DEPTH` of them, 4 by default).

### The stack

The stack isn't in the figures above, and it matters more than its size suggests: the Pico SDK reserves 2 KB for `main` (`PICO_STACK_SIZE`), and nothing stops a program going past it. The stack quietly grows into the memory below, which works fine right up until the day it doesn't.

So QD4P keeps big things off the stack (that's why its buffers are `static`), and [`tests/size/stack.py`](../tests/size/stack.py) works out the most each entry point can use, from the compiler's own figures for every function:

| Calling | Uses at most |
|---|---:|
| `qd_roll_run`, `qd_reroll_*`, `qd_set_run`, `qd_mass_run` | 0.5 KB |
| `qd_odds` | 0.7 KB |
| `qd_fair_start`, `qd_fair_begin` | 0.8 KB |
| `qd_parse` | 1.0 KB, with brackets nested 8 deep (the most allowed); 0.5 KB without brackets |
| `qd_expand`, `qd_roll_text`, `qd_pick_range` | 1.3 KB, the same way |
| `tbl_roll` (the example table system) | 1.6 KB four links deep; 2.1 KB if a table's dice also nest brackets 8 deep |
| `qd_sys_outcome`, `qd_deck_deal`, `qd_stats_report` | 0.1 KB |

Your own `main` and whatever calls QD4P come on top. If you need more room: `PICO_STACK_SIZE` in your build raises the reservation, and `QD_MAX_NESTING` (brackets) and `TBL_DEPTH` (links) lower the worst cases.

## RAM: what your program declares

These are yours: declare the ones you use, as many as you need. `static` or global is better than a local variable for the big ones: the default stack is 2 KB, and a `qd_result_t` alone is bigger than that.

| Structure | Bytes (defaults) | Holds | Sized by |
|---|---:|---|---|
| `qd_roll_t` | 1,408 | a compiled roll: pools, modifiers, maths, target | `QD_MAX_POOLS`, `QD_MAX_ROLL_OPS`, `QD_MAX_BUCKETS` |
| `qd_result_t` | 2,212 | a roll's result: every die, totals, counts | mostly 12 bytes x `QD_MAX_KEPT_DICE` |
| `qd_parsed_t` | 1,616 | a parsed roll: a `qd_roll_t` plus its label, comment, repeat | |
| `qd_outcome_t` | 128 | what a result means: "Hard success", "2 raises" | |
| `qd_parse_error_t` | 68 | where and what a notation error is | |
| `qd_set_result_t` | 108 | a set's values and which were kept | `QD_MAX_REPEAT` |
| `qd_mass_result_t` | 48 | a mass roll's counts | |
| `qd_die_t` | 248 | a custom die and its faces | `QD_MAX_FACES` |
| `qd_deck_t` | 40 | a deck: which cards are out, the reshuffle rules (the cards themselves stay in flash) | |
| `qd_odds_t` | 1,184 | odds: the distribution and outcome chances | `QD_ODDS_BINS` |
| `qd_stats_t` | 7,048 | stats for nerds: counts per face, per kind of die | `QD_STATS_KINDS` x `QD_STATS_FACES` |
| `qd_expand_info_t` | 452 | the working of `qd_expand`: each `{roll}` and its value | `QD_EXPAND_ROLLS` |

(Measured with the Pico's own compiler, arm-none-eabi-gcc for Cortex-M33.)

A typical dice app holds one `qd_roll_t` and one `qd_result_t` per roll it shows at once: about 3.5 KB each.

## Making it smaller

Every limit is a setting in [`qd4p/qd_config.h`](../qd4p/qd_config.h). Change one from your `CMakeLists.txt`, on the library and marked `PUBLIC`, so the library and your program agree on the sizes:

```cmake
target_compile_definitions(qd4p PUBLIC QD_MAX_ROLL_DICE=100 QD_MAX_KEPT_DICE=50)
```

The settings that matter most:

| Setting | Default | What it costs | Smaller means |
|---|---:|---|---|
| `QD_MAX_ROLL_DICE` | 1000 | 16 bytes each, in the engine's working space | a roll needing more dice, explosions included, is refused with an error |
| `QD_MAX_KEPT_DICE` | 100 | 12 bytes each, in every `qd_result_t` | past it, results are counted rather than listed die by die (totals stay exact) |
| `QD_ODDS_SPAN` | 1024 | 16 bytes each, in the odds tables | exact odds of sums wider than this are simulated instead |
| `QD_ODDS_BINS` | 256 | about 8 bytes each, in `qd_odds_t` and the odds tallies | very wide distributions are grouped more coarsely |
| `QD_STATS_KINDS`, `QD_STATS_FACES` | 16, 100 | about 4 bytes per face per kind, in `qd_stats_t` | fewer kinds of dice tracked at once |
| `QD_MAX_POOLS` | 8 | about 130 bytes each in a `qd_roll_t` and 120 in a `qd_result_t` | fewer kinds of dice in one roll (`1d20 + 1d4 + 2d6` is three) |

Measured: `QD_MAX_ROLL_DICE=100` with `QD_MAX_KEPT_DICE=50` takes the roll engine's RAM from 15.7 KB to 1.6 KB and each `qd_result_t` from 2,212 to 1,612 bytes. For the Dice Roller's own screens, 100 dice is plenty: a wargame's biggest volley is a few dozen.

Flash doesn't shrink with these settings; it shrinks by not calling things. Building rolls in code instead of parsing notation saves the parser's 15 KB and, unless you judge rolls, the game systems' 5 KB.

## Tables, text and other data

Data costs flash like code does, and it all comes out of the same 4 MB as your pictures and sounds. Some yardsticks:

- **A table entry written into the app** (`tables_own.c`) costs its text plus about 12 bytes: the text, a pointer to it, and its range.
- **The example table system's asset** costs, per entry, its text plus 11 bytes (range, kind, count, the end of the text), and per table, its name and dice plus 6 bytes. The two 12-entry tables of the size check are 803 bytes.
- **The decks' cards** are in flash already, with the deck code: about 1.5 KB for poker and tarot together, names included.
- **The fair-mode word list** is about 2.4 KB (the 256 words, and a table pointing to them), linked only with fair mode.

If space gets tight, tables and text are a good thing to move into an asset pack in flash alongside the pictures (QM4P's QA4P does this), so they can change without rebuilding the program.

## Measuring your own program

[`tools/size_report.py`](../tools/size_report.py) reads the map file the Pico SDK writes beside every program, and lists what QD4P takes, file by file:

```
python3 tools/size_report.py build/my_app.elf.map
```

```
                                       flash       RAM
qd4p:                                  15171     16038
    qd_roll                             9213     16012
    qd_sys                              4429         0
    qd_rng                               888        26
    qd_dice                              557         0
    qd_entropy_pico                       84         0
program:                                1842      3620
pico-sdk:                              29334      5884
C library:                              2646         0
TOTAL                                  48993     25542

QD4P: 14.8 KB of flash and 15.7 KB of RAM of this program's 47.8 KB and 24.9 KB
```

That is `tests/size/systems.c`: five rolls built in code and judged.

Only what is linked is counted, so it shows what this program pays, not what the library could cost.
