# 10. How it works

For anyone who wants to know why, not only how. None of this is needed to use QD4P; all of it is in the source, which is commented to be read.

[The layers](#the-layers) · [Paying only for what you use](#paying-only-for-what-you-use) · [The generator](#the-generator) · [Seeding and stirring](#seeding-and-stirring) · [Unbiased ranges](#unbiased-ranges) · [The roll engine](#the-roll-engine) · [The parser](#the-parser) · [Game systems](#game-systems) · [Odds](#odds) · [The fairness check](#the-fairness-check) · [Fair mode](#fair-mode) · [Memory](#memory)

---

## The layers

```
  your code       your app
                     |             |               |
  optional        parser       game systems     odds, stats, dice in text,
                  (text to     (shortcuts and   fair mode
                  a roll)      outcomes)
                     |             |               |
  core            the roll engine: pools, keep and drop, explode, reroll,
                  counting, buckets, the sum
                     |
                  dice and decks
                     |
                  the generator: seeding, stirring, unbiased ranges, replay
  Pico only       entropy sources: the TRNG, ADC noise, button timing
```

An app can call any layer directly. One that builds its rolls in code goes straight to the engine and never touches the parser. Only the bottom row knows it's on a Pico, which is why everything else builds and is tested on a PC: millions of rolls are checked against their expected distributions before anything reaches the device.

---

## Paying only for what you use

The Pico SDK compiles every function and every variable into its own section (`-ffunction-sections -fdata-sections`), and the linker throws away every section nothing refers to (`--gc-sections`). So the library can be generous, and a program still carries only what it calls.

QD4P is written to keep that working. Features don't call each other unless they need to: dealing a card never touches the roll engine; rolling dice never touches the parser. Working buffers are declared inside the function that uses them, so they leave with it. Fair mode, for example, costs a program that doesn't use it 8 bytes: a few checks in the generator for whether a session is holding it. [What QD4P costs](../SIZES.md) measures every feature.

---

## The generator

**xoshiro128\*\*** (Blackman and Vigna): 128 bits of state, 32 bits per number, a period of 2<sup>128</sup> - 1, and it passes the standard statistical test batteries. Each number costs a handful of shifts, XORs and one multiply: a few nanoseconds on the Pico 2.

Why not the RP2350's hardware generator for every number? It's slower, and it can't do **replay**: the same seed giving the same rolls on any machine, which testing and bug reports depend on. So the hardware does what it's best at, starting the generator somewhere nobody can guess, and xoshiro does the rest.

The one rule of xoshiro is that its state must never be all zeros: it would stay at zero forever, which is a very consistent dice roller and not a useful one. Every function that sets the state checks for it.

---

## Seeding and stirring

**At boot**, [`qd_rng_init_hw`](reference/randomness.md#qd_rng_init_hw) fills the state from the RP2350's true random number generator, which measures electrical noise.

**Stirring** mixes anything else in: a word the DM types, sensor noise, the microsecond of a button press. Each 32-bit piece goes through MurmurHash3's finaliser (a mixing function in which every input bit flips about half the output bits), is XORed into the state, and the generator is then stepped eight times so the new bits spread through all 128 bits of state.

**Why stirring can't make things worse:** XOR with something independent of the state is a one-to-one shuffle of possible states. If the state was unpredictable before, it's exactly as unpredictable after, whatever was stirred in, even "aaaa". If what was stirred in is unpredictable too, the state is now more so. That's why a doubtful source is safe to add, and why every button press is worth stirring.

**Replay mode** replaces the state with one built from a seed by splitmix64, and ignores stirring until it's turned off, since stirring would break the replay.

---

## Unbiased ranges

Turning 32 random bits into a d6 has a trap in it. `x % 6` is very slightly unfair: 2<sup>32</sup> isn't a multiple of 6, so four of the 4,294,967,296 possible values have nowhere fair to go, and the low faces come up a hair more often. A hair, but dice are about fairness, and the d1000 and the d30 have bigger hairs.

QD4P uses **Lemire's method**: multiply the 32-bit value by n as a 64-bit number, and the top 32 bits are a value from 0 to n - 1. The few values that would make some results more likely are recognised from the bottom 32 bits and rolled again. For a d6 that happens 4 times in 4 billion, so it's one multiply and almost never a division.

---

## The roll engine

A roll is **pools** (dice of one kind and what to do with them) and a **sum**, kept as a tiny program for a stack machine: push pool 0's total, push 2, multiply, push pool 1's total, add. The parser compiles text into it; the builders write it directly.

Running a roll goes through fixed **stages** whatever order the modifiers were given in: roll and set aside (advantage), explode / implode / reroll in the order written, keep or drop, total, the sum, then counting. [The roll engine](reference/rolls.md#how-a-roll-runs) lists them. Fixed stages are what make `10d6 e6 k8` mean the same thing however it's typed.

**One working buffer** holds every die of the roll as it's rolled, up to 1,000 (`QD_MAX_ROLL_DICE`), with what happened to each: which die it exploded from, whether it was dropped. Keep and drop sort the dice by value with a bottom-up **merge sort**, which is stable: dice of equal value keep the order they were rolled in. That's how ties are always settled the same way ("lowest" is the earliest, "highest" the latest), and it sorts 1,000 dice in about 10,000 steps. Rolls without keep or drop aren't sorted at all.

**Summary mode**: a result lists up to 100 dice (`QD_MAX_KEPT_DICE`). Past that, the rest are rolled and counted (totals, face counts, minimum, maximum) but not listed, so a `1000d6` costs 2 KB of result, not 12.

**Caps**: one die explodes at most 100 times, and an explode-forever that every face would trigger is refused when the roll is built. A dice roller that hangs because someone typed `1d1 ie` is a dice roller that gets thrown across the room.

---

## The parser

A **recursive-descent parser**: one function per piece of grammar (a sum is products joined by `+` and `-`; a product is values joined by `*` and `/`; a value is a number, dice, or a bracketed sum), each calling the next. Precedence comes free from that shape. [The grammar](05-notation.md#grammar) is in the notation guide.

Keywords are matched longest first (`irg` before `ir` before `i`), so modifiers can run together without spaces. Game words like `atk` are only game words when they end there (or run into `v` / `vs`), so `atkins` isn't an attack.

**Errors** are the parser's other job, and probably the more important one. Every error records the column and length of the problem and says what's wrong in words, often with the fix: "no space inside dice: write 4d6", "to roll it 400 times, write 400 x ...". The first error stops the parse: a parser that tries to guess what you meant is a parser that rolls the wrong dice confidently.

Each level of brackets costs the parser 72 bytes of stack, up to 8 levels (`QD_MAX_NESTING`): the code that reads dice is kept out of the recursive path for that reason.

---

## Game systems

A game system is two things: a **builder** that sets up the game's dice (D&D's d20 with advantage, Savage Worlds' trait and wild die with "the better one" in the sum), and a **reading** of the result by the game's rules. The roll itself is ordinary: the engine doesn't know which game it's rolling for. The builder notes the system and its numbers (the skill, the hunger dice) in the roll, and [`qd_sys_outcome`](reference/systems.md#qd_sys_outcome) reads them back.

Every reading has a **level**, ordered so higher is better and below zero is failure, so an app can treat all games alike ("green above zero") and still name each result properly.

---

## Odds

Four methods, the first that fits ([the reference](reference/odds.md#how-the-odds-are-worked-out) says which rolls use which):

**1. Convolution.** For plain sums: the chance of each total of two dice is every way the first die's values and the second's can add up to it, each weighted by both chances. Do it again for the third die, and so on. It's exact, and fast: 100d6 is 100 passes over at most 600 values. The chances are kept in single precision and put back to a total of exactly 1 after every step, which holds the averages to about one part in a million.

**2. Trying every roll.** For anything else small enough, the generator is **scripted**: instead of random numbers, it returns a chosen sequence, and the roll is run once for every possible sequence, like an odometer turning over. Each sequence's chance is the product of 1/n for each of its draws. `4d6k3` is 1,296 runs, `atk adv` 400. It's exact, and it works for anything the engine can roll, keep, drop and game readings included, because it *is* the engine, rolling every possible way.

**3. The bell curve.** A plain sum of 20 or more dice too wide for convolution (`1000d6666`) uses the normal distribution: the average and the spread are exact (each die's, added up), and the chance of each value is the area under the bell curve between value - ½ and value + ½. For that many dice, the central limit theorem keeps it within a fraction of a percent.

**4. Simulation.** Everything else is rolled 10,000 times and counted. With that many rolls, a chance near 50% is usually right to within a percentage point.

Throughout, the generator is saved first and restored after, and stats for nerds stops watching, so asking for odds never changes the dice that follow.

---

## The fairness check

A **chi-square test**: for each face, (rolled - expected)² / expected, added up. A fair die gives a small number, since the faces come up about as often as expected. The question is how big is too big, and that depends on how many faces the die has (its **degrees of freedom**, faces - 1).

QD4P draws the line where a fair die crosses it 1 time in 1,000 (p = 0.001). The limits are the standard table values up to 30 degrees of freedom, and the Wilson-Hilferty approximation beyond, within 0.3%.

The test needs about 5 expected rolls per face to be trusted, hence "too few" before then. For a weighted die, "expected" comes from its weights, so a loaded die is judged on whether it's loaded the way it says.

---

## Fair mode

Each action's dice come from SHA-256 of the session key and the action's number: the first 16 bytes of the hash become the generator's state. SHA-256 is a cryptographic hash, which means two things here. Nobody can work back from the fingerprint (a hash of the key) to the key, so revealing the fingerprint at the start gives nothing away. And nobody can find a second key with the same fingerprint, so the key revealed at the end must be the one locked in at the start.

[Part 7](07-fair-mode.md#how-it-works) has the exact recipe, for anyone writing their own checker.

---

## Memory

QD4P never calls `malloc`. Every structure has a size fixed at build time by [the settings](appendices.md#settings), so memory use is known before the program runs and nothing can run out at the table. The big things (a result, a parsed roll, the odds tables) are meant to be `static`: the Pico's stack is 2 KB, and a `qd_result_t` alone is 2.2 KB.

The library's own buffers are `static` too, which has one consequence: **one roll at a time**. QD4P is not meant to be called from both cores at once, or from an interrupt while the main program is rolling. A dice roller has no reason to do either. [What QD4P costs](../SIZES.md) lists every buffer, and the stack each function needs.
