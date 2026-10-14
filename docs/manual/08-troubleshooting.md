# 8. Troubleshooting

Something's wrong. Find it here; each answer starts with the most likely cause.

[The console says nothing](#the-console-says-nothing) · [The same rolls every time](#the-same-rolls-every-time) · [The dice seem streaky](#the-dice-seem-streaky) · [A roll is refused](#a-roll-is-refused) · [The total is 0](#the-total-is-0) · [Only 100 dice are listed](#only-100-dice-are-listed) · [Building](#building) · [Crashes and hangs](#crashes-and-hangs) · [Odds are slow](#odds-are-slow) · [Fair mode doesn't check out](#fair-mode-doesnt-check-out) · [Out of memory](#out-of-memory)

---

## The console says nothing

- **Press Enter.** The console prints its prompt when it starts, which is usually before your terminal has connected; it doesn't print it again until you press a key.
- **The wrong port.** The Pico appears as a USB serial port only while a program with USB serial is running. In BOOTSEL mode (the `RP2350` drive) there is no port. On Linux it's usually `/dev/ttyACM0`, on a Mac `/dev/tty.usbmodem...`, on Windows a COM port in Device Manager.
- **Another program has the port.** Only one program can hold a serial port: close the other terminal, or the other VS Code window's Serial Monitor.
- **The cable.** Some USB cables carry power and no data. If the `RP2350` drive appears in BOOTSEL mode, the cable is fine.
- **Typed keys show twice, or not at all.** The console echoes what you type; turn off your terminal's local echo. (On a PC, the PC console doesn't echo, because a terminal does.)

## The same rolls every time

**Replay mode is on.** `rng` in the console says so. Something called [`qd_rng_seed_fixed`](reference/randomness.md#qd_rng_seed_fixed), or someone typed `seed 42`; `live` (or [`qd_rng_init_hw`](reference/randomness.md#qd_rng_init_hw)) ends it.

**Or the generator was never seeded.** Without [`qd_rng_init_hw`](reference/randomness.md#qd_rng_init_hw) at boot, the generator starts from the same fixed state at every power-up. Call it once, at the start of `main`.

## The dice seem streaky

They probably aren't. Real randomness is lumpier than people expect: three 1s in a row on a d20 happens 1 time in 8,000 for any three particular rolls, which across a long evening at a table of five is close to certain to happen to someone. Streaks are what randomness looks like.

To check: [stats for nerds](06-odds-and-stats.md#is-my-die-cursed). `stats 20` after a session shows a d20's faces and a fairness check; `test 20 1000000` rolls a million and checks them. If the electronic dice really were off, a million rolls would show it plainly.

## A roll is refused

[`qd_roll_run`](reference/rolls.md#qd_roll_run) returns an [error](reference/rolls.md#qd_err_t), and [`qd_err_text`](reference/rolls.md#qd_err_t) says what it is in words. The usual ones:

| Error | Usual cause | Fix |
|---|---|---|
| `QD_ERR_FULL` | more than 8 pools, 4 explode/implode/reroll steps on a pool, 8 buckets or 24 steps in the sum | fewer; or raise the [setting](appendices.md#settings) |
| `QD_ERR_TOO_MANY_DICE` | more than 1,000 dice (`QD_MAX_ROLL_DICE`) | fewer dice, or a [mass roll](reference/parse.md#qd_mass_run) for many repeats |
| `QD_ERR_FOREVER` | explode or reroll forever on every face: `1d6 ie1` | a test some faces fail |
| `QD_ERR_NO_DICE` | a pool of 0 dice, or a 0-sided die | |
| `QD_ERR_PERCENT` | percentile dice with explode, reroll or advantage | bonus and penalty dice are their advantage |
| `QD_ERR_DIGITS` | digit dice exploding or rerolling | |

Notation that won't parse says where and why: [the list of errors](05-notation.md#errors).

## The total is 0

**The roll was refused**, and the result cleared. Check what [`qd_roll_run`](reference/rolls.md#qd_roll_run) returned.

**A game-system roll built into a roll still in use**? Since 1.0, every [builder](reference/systems.md) starts the roll afresh, so this can't happen; with an older copy of the library, call `qd_roll_init` first.

**A roll that counts successes** (`6d10 t8`) puts them in `res.successes`, and `QD_RES_COUNTING` in `res.flags` says so. Its `res.total` is the sum of the dice, which is rarely what anyone wants to show.

## Only 100 dice are listed

That's **summary mode**: past 100 dice (`QD_MAX_KEPT_DICE`), the rest are rolled and counted but not listed one by one. Every number is still exact: total, successes, buckets, face counts (`res.pools[k].face_count[]`), lowest and highest. `QD_RES_SUMMARY` in `res.flags` says it happened. Raise the setting if you need every die listed; each one costs 12 bytes in every `qd_result_t`.

## Building

- **"undefined reference to `qd_...`"**: the program doesn't link the library. Add `qd4p` to `target_link_libraries` ([CMake in five minutes](09-getting-started.md#cmake-in-five-minutes)).
- **"qd4p.h: No such file"**: the same cause; linking `qd4p` also adds its folder to the include path.
- **"undefined reference to `tbl_...`"**: the example table system isn't part of the library. Add `examples/tables/table.c` to your `add_executable`, or copy it into your project.
- **"multiple definition of `qd_platform_seed`"**: both `qd_entropy_pico.c` and `qd_entropy_host.c` are in the build. On a Pico, only the first; on a PC, only the second.
- **"hardware/adc.h: No such file" on a PC**: `qd_entropy_pico.c` is Pico-only. Leave it out of PC builds (the PC tests and the PC console do).

## Crashes and hangs

**A big structure on the stack.** The Pico's stack is 2 KB, and a `qd_result_t` is 2.2 KB, a `qd_parsed_t` 1.6 KB, a `qd_odds_t` 1.2 KB. Declared as local variables they run off the end of the stack, quietly, and the program crashes later somewhere unrelated. Make them `static` or global. [The stack](../SIZES.md#the-stack) lists what each QD4P function needs.

**Two things rolling at once.** QD4P keeps its working space in `static` buffers: roll from one core, and not from an interrupt handler while the main program might be rolling.

## Odds are slow

Odds that have to be **simulated** take the longest: exploding dice (Savage Worlds) about half a second on a Pico 2. The answer for a given roll never changes, so work it out once and keep it, rather than every time the screen redraws. Exact odds of small rolls take milliseconds. [How the odds are worked out](06-odds-and-stats.md#how-the-odds-are-worked-out).

## Fair mode doesn't check out

- **The fingerprint doesn't match:** a key word written down wrong, or two words swapped. Every letter typed has to match, so when a word is hard to read, type only its first three letters: they're enough, and no two words share them. The order matters too: the console shows the key in two lines of eight, in order.
- **An action gives different dice:** the roll typed differently (`3d6` isn't `3d{custom}`, `4d6k3` isn't `4d6`), the wrong number, or, for a reroll or a deal, the actions before it not typed first ([what each action needs](07-fair-mode.md#what-each-action-needs-to-be-checked)).
- **"#number is for checking a fair session"**: `#14 ...` only works after `fair check` and the key. During a live session, numbers are given, not chosen.

## Out of memory

The defaults are sized for a busy table with room to spare. [What QD4P costs](../SIZES.md) lists every buffer and the settings that shrink them; `QD_MAX_ROLL_DICE=100` alone saves 14 KB.
