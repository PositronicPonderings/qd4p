# QD4P: dice library design

Design snapshot, 2026-10-12. The living version is the "QD4P: dice library
design" doc in the Dice Roller project; this copy travels with the code.

## Goals and principles

QD4P (QuickDice 4 Pico) is a reusable C library for rolling dice, drawing
cards and interpreting results for tabletop games. The Dice Roller is its
first user, but nothing in it is specific to that device.

- **Pay only for what you use.** Each layer (randomness, dice and decks, roll
  engine, parser, game systems, extras) is a separate part. An app that only
  rolls 2d6 links only the generator and the engine.
- **Portable core.** Everything except the hardware entropy sources is plain
  C11 with no Pico dependency, so the whole library builds and is tested on a
  PC: millions of rolls to check distributions before anything touches the
  device.
- **No surprises at the table.** No dynamic memory, hard caps on dice,
  explosions and work per roll, and readable error messages with the
  position of the mistake.
- **Honest randomness.** Unbiased number picking, hardware-seeded, with user
  and sensor input that can only add randomness, never remove it.
- **The library owns game outcomes.** Raises, CoC success levels, VtM messy
  criticals and the like are computed in QD4P, so every app gets them right
  the same way.
- **Inspired by Dice Maiden, not bound to it.** The notation is QD4P's own
  documented dialect.
- **Teaching-quality code.** Heavily commented, with an old-style manual:
  every function has an entry and a working example.

## Architecture

```
  your code       Your app (the Dice Roller)
                     |             |              |
  optional        Parser      Game systems      Extras (rerolls, sets, tables,
                  text to     shortcuts and     odds, stats, fair mode)
                  compiled    outcomes
                  roll
                     |             |              |
  core,           Roll engine: pools, keep/drop, explode, reroll, counts,
  portable C        buckets, summary mode
                     |
                  Dice and decks: standard, custom and composite dice; decks
                     |
                  Randomness (qd_rng): seeded generator, unbiased ranges,
                     |                 stirring, replay
  Pico only       Entropy sources: RP2350 TRNG, ADC noise, encoder jitter,
                  your sensor
```

An app can call any layer directly: a simple app builds rolls in C and goes
straight to the engine without the parser. Only the bottom row touches Pico
hardware.

## Randomness

One generator (`qd_rng`): xoshiro128**, seeded from the RP2350's hardware
random source at boot. Users and sensors can stir extra entropy in at any
time; nothing can make it less random than the hardware seed alone.

- **Unbiased picks.** `qd_rng_range(n)` returns 0..n-1 with every value
  equally likely, using rejection rather than `% n`.
- **Entropy sources** share one interface, so new randomness hardware is one
  file:

  ```c
  typedef struct {
      const char *name;                 /* "Light sensor" - shown in menus */
      bool (*begin)(void *ctx);         /* start collecting (optional) */
      uint32_t (*sample)(void *ctx);    /* one raw reading; may be poor quality */
      void (*end)(void *ctx);           /* stop collecting (optional) */
      void *ctx;
  } qd_entropy_source_t;

  void qd_rng_stir(const void *bytes, size_t len);
  void qd_rng_stir_source(const qd_entropy_source_t *src, uint32_t samples);
  ```

  Built-in: RP2350 TRNG, ADC noise on a spare pin, button/encoder timing
  jitter. Samples are hashed into the state, so even a mostly predictable
  source can only help.
- **User seed:** stirred in like any other entropy.
- **Replay mode:** `qd_rng_seed_fixed(seed)` makes every roll reproducible,
  for tests, bug reports and teaching. The app should show when it is on.
- **Fair mode (v1, the lighter promise):** at session start a secret
  128-bit key is chosen and its fingerprint shown. Rolls are derived from
  the key and a roll counter. At session end the key is revealed; anyone
  with the PC tool can replay a recorded session (the rolls written down at
  the table, in order) and confirm every result. Exporting the device's own
  roll log is a later option. Stirring is disabled while fair mode is on.

## Dice and decks

| Kind | Described by | Examples |
| --- | --- | --- |
| Standard die | sides only, faces 1..N | d4 ... d100, DCC's d3/d5/d7/d14/d16/d24/d30 |
| Custom die | up to 30 faces: value, weight (1-255), optional 4-char label | Fudge, loaded d6, average die |
| Digit dice | dice read as digits, each its own die | `dd66` (11..66, two d6), `dd666`, `dd36`; Year Zero and Traveller tables. `d66` stays a single die, 1..66 |
| Percentile | units die plus tens dice | d100, CoC bonus/penalty tens dice |
| Deck | up to 255 cards: value, suit/group, label; dealt/discard state | poker with jokers, tarot, custom |

- **Faces, not just values:** every die records which face came up, so
  labels, duplicate values and non-numeric faces stay exact, and stats can
  compare against each face's expected share.
- **Weights:** cumulative-weight lookup; all-weights-1 dice take a fast path.
- **Decks:** draw (top or random), deal, discard, return a card, shuffle the
  discards back. State is a dealt-card bitmap, matching the Dice Roller
  settings store's DECK record.
- **Reshuffle policy, per deck:** every draw (each card goes back), on demand
  only, or on condition (deck empty; before the next draw after a chosen card
  has come up, e.g. a joker in Savage Worlds). Conditions combine.
- **Built-in decks:** poker (52 plus a chosen number of jokers) and tarot
  (78). Savage Worlds = poker with two jokers and the joker reshuffle rule.
- **Same data as the settings library:** a custom die has exactly the fields
  of the settings LIB_DIE record.

## Roll engine and results

Runs a compiled roll (built by the parser or in C); never touches text.

**Stages, in fixed order:** roll the pool (with advantage/disadvantage
extras); explode, implode and reroll in the order written; keep/drop; total
and maths; count (successes, failures, botches, doubles, sets); categorise
and work out the system outcome.

**A result holds:** every die (group tag, die type, face index, value, flags:
exploded-from, dropped, rerolled, imploded, crit, fumble); totals and the
system outcome (enum plus label); bucket counts; and a DM-only /
player-visible mark on each field.

**Ties and order.** When dice tie, "lowest" picks the earliest-rolled and
"highest" the latest-rolled, so which die is marked dropped never varies.
Dice made by explosions and rerolls go on the end of the pool's list with
the index of the die they came from; a step never visits dice it made
itself. The sum is a small stack program (pool totals, numbers, + - * /,
negate), which the parser fills in with proper precedence; the C builders
work left to right. Division rounds toward zero.

**Digit dice** (`dd66`) are a kind of pool: one draw per digit, 1-9 sides
each, up to 4 digits. They can be kept, dropped, counted and bucketed but
not exploded or rerolled.

**Buckets:** up to 8 user-defined tests on a die (comparison, range or list,
on value or label), plus "everything else". None are built in; `<3`, `=6`
etc. are illustrations.

| Limit | Default | Past it |
| --- | --- | --- |
| Dice kept individually | 100 | Summary mode: count, sum, min, max, mean, count per face; totals, successes and buckets stay exact |
| Dice per roll, incl. explosions | 1,000 | Roll refused with an error |
| Explosions per die | 100 | Chain stops; result flagged |
| Pools per roll | 8 | Roll refused |
| Explode/implode/reroll steps per pool | 4 | Step refused |
| Terms in the sum | 24 | Roll refused |

All are compile-time settings in `qd4p/qd_config.h`.

## Dice notation and the parser

`qd_parse("4d6k3", &roll, &err)` compiles text once; the engine runs the
compiled form every time. Apps that only use built-in rolls can leave the
parser out. v1 notation:

| Feature | Notation | Example |
| --- | --- | --- |
| Dice | `NdS` `dS` `d%` `dF` | `3d6`, `d66` (1..66), `4dF` |
| Digit dice | `ddXY...` one die per digit | `dd66` (tens d6, units d6), `dd666` |
| Custom die | `d{name}` | `2d{Loaded d6}` |
| Maths | `+ - * /`, brackets, extra dice | `(1d8+2)*2`, `1d20+1d4` |
| Keep / drop | `kN` `klN` `kmN` `dN` `dhN` | `4d6k3`, `2d20kl1` |
| Advantage | `advN` `disN` | `1d20 adv1` |
| Explode | `e` `eN` `ieN` | `1d6 ie` |
| Implode | `i` `iN` | `1d10 i1 e10` |
| Reroll | `rN` `irN` `rgN` `irgN` | `2d6 r1` |
| Success counting | `tN` `tlN` `fN` `bN` `dsN` | `6d10 t8 f1` |
| Matching sets | `sets` | `5d10 sets` |
| Buckets | `[<3, =6]` | `100d6+2 [<3, =6]` |
| Groups | `name:` | `base:4d6 t6 + stress:2d6 t6` |
| Repeat, multi-roll | `N x roll`, `;` | `6 x 4d6k3` |
| Label | `(Label)` | `(Bite) 1d20+14` |
| System shortcut | `name args` | `sw8`, `coc 65 b2`, `vtm 6 h2` |

**Percentile bonus and penalty dice (CoC):** `d% b2` rolls two extra tens
dice and keeps the best, `d% p1` one extra and keeps the worst, up to 3 each
way; bonus and penalty cancel one for one. Every tens die stays in the
result, marked kept or dropped.

Errors name the column and the problem; the parser enforces the limits
before anything runs. The parser also builds on the PC, so `settings.py` can
check roll expressions.

## Outcomes and game systems

A system is a shortcut (expands to core notation) plus an outcome rule.
Building blocks: target and margin, raises, degrees (bands or fractions of a
skill), crit/fumble rules, opposed rolls.

Built-in v1 systems (decided; others are user-defined or later):

| System | Shortcut | Outcome |
| --- | --- | --- |
| Generic | notation | total, successes, buckets |
| D&D 5e | `d20 +5 adv` | pass/fail vs DC, natural 20/1 |
| Savage Worlds | `sw8`, `sw8 w` | success, raises, snake eyes, aces |
| Call of Cthulhu 7e | `coc 65`, `b1-b3`/`p1-p3` | Fumble / Fail / Regular / Hard / Extreme / Critical |
| FATE | `fate +2` | ladder result |
| Vampire 5e | `vtm 6 h2` | successes, critical, messy critical, bestial failure |
| DCC | `dcc d24+2` | total; dice-chain helpers |

Mass rolls use the same rules and return counts.

## Extras

- **Rerolls:** `qd_reroll(&result, which)` rerolls chosen dice and recomputes;
  the original is kept. Bennies, pushes, luck.
- **Roll sets:** named lists of rolls with optional rules (`6 x 4d6k3`).
- **Random tables:** a die expression plus ranged entries, nesting to a depth
  cap. Text files converted into an asset pack on the PC (decided for v1);
  QD4P only needs a pointer to the table bytes.
- **Odds:** `qd_odds(&roll, test)`; exact for sums of dice, otherwise 10,000
  simulated rolls (marked approximate).
- **Stats for nerds:** per die type count, sum, sum of squares, min, max,
  face histogram; mean, standard deviation and a chi-square "is this die
  behaving?" check.

## Repo, footprint and docs

- Own repo, `PositronicPonderings/qd4p`, no dependency on QM4P; demos and
  tests use a USB console. Apps bring QD4P and QM4P together.
- Prefix `qd_`, licence MIT-0, AI disclosure via AI_DISCLOSURE.md and SPDX
  tags, no credit lines in code.
- CMake library target for pico-sdk projects plus a plain host build for
  tests and PC tools. Pico-only code: the entropy sources (`*_pico.c`).
- Optional parts switch on in CMake; each system is its own file.
- No malloc. Integers for rolls; floats only in stats and odds.
- Host tests on every change: statistical tests, golden results in replay
  mode, limit tests.
- Manual in the QG4P style.

## Milestones

1. **D0 Skeleton:** repo, CMake for Pico and host, test runner, USB console.
2. **D1 Randomness:** generator, unbiased ranges, entropy sources, replay.
3. **D2 Dice and decks.**
4. **D3 Roll engine** (rolls built in C). First point the Dice Roller can
   use. Done: `qd_roll.h`.
5. **D4 Parser.**
6. **D5 Outcomes and systems.**
7. **D6 Rerolls and roll sets.**
8. **D7 Stats for nerds and odds.**
9. **D8 Random tables.**
10. **D9 Fair mode.**
11. **D10 Manual and v1.0.**

## Still open

- Where custom decks are stored: the settings library store (16 KB slot) or
  the asset pack.
