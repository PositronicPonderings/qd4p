# Fair mode

Fair mode lets the players check, after the session, that the dice roller
didn't change anything during it. Nothing is hidden from them except a
secret key, and the key is revealed when the session ends.

- [At the table](#at-the-table)
- [Checking afterwards](#checking-afterwards)
- [What it proves, and what it doesn't](#what-it-proves-and-what-it-doesnt)
- [What each action needs to be checked](#what-each-action-needs-to-be-checked)
- [In a program](#in-a-program)
- [How it works](#how-it-works)

## At the table

1. **Start.** The DM turns fair mode on (`fair` on the console). The device
   shows a **fingerprint**: six words, like

   ```
   baker canyon dove rose cycle orange
   ```

   Everyone who wants to check later writes it down **now**, before
   anything is rolled. It is the device's promise: it locks in the key
   without revealing it.

2. **Play.** Every roll, reroll, deal and table roll is an **action**, and
   each one gets the next number: `fair #1`, `fair #2`, ... A player who
   wants to check a roll writes down its number, what was rolled and what
   came up:

   ```
   #14  atk +5 vs 15   rolled 19, total 24, hit
   ```

   Nobody has to write everything down. Each action can be checked on its
   own.

3. **End.** The DM ends the session (`fair end`). The device shows the
   **key**: sixteen words. Write them down, or take a photo:

   ```
   wood cherry shell shoe tower gate coffee nest
   piano butter ankle echo wizard lemon vase baker
   ```

While fair mode is on, everything that would normally change the
generator is ignored: stirring in entropy (`stir`, `trng`, `adc`, `tap`),
hardware reseeds and replay mode (`seed`). Starting a new session while one
is running is refused too.

## Checking afterwards

The checker is the same console, built for a PC from the same source and
library as the device, so the two can't disagree:

```
sh examples/pc/build.sh          # Linux, macOS or WSL; writes examples/pc/qd4p_pc
examples/pc/qd4p_pc
```

Then:

```
> fair check wood cherry shell shoe tower gate coffee nest piano butter ankle echo wizard lemon vase baker
checking with this key. Its fingerprint:
  baker canyon dove rose cycle orange
```

**First, compare the fingerprint** with the one written down at the start.
If it matches, this key is the one the device locked in before the first
roll. (Each word can be cut to its first three letters: `woo che she ...`.
No two words share their first three letters.)

**Then type each action with its number**, in any order:

```
> #14 atk +5 vs 15
fair #14
atk + 5 vs 15
  1d20: 19  = 19
  total 24
  => Hit
```

Same dice, same result: that roll is confirmed. A different result means
something changed: a typo in the roll or the number, a key word written
down wrong, or a device that cheated.

`fair end` stops checking. The console on the device can check as well,
with the same commands.

A file of commands works too: `examples/pc/qd4p_pc < session.txt` runs
each line and prints the results, with each line echoed so the log reads
like the session.

## What it proves, and what it doesn't

**It proves the device didn't change anything during the session.** The
fingerprint locks the key in before the first roll; every action's dice
come only from the key and the action's number. So after a roll the device
can't:

- swap the result for another one (the check would show the real dice),
- quietly roll again until it liked the answer (the action number would
  have moved on, and anyone checking that number gets the first result),
- switch to a different key part way through (the fingerprint wouldn't
  match).

**What it doesn't prove:** the key is the device's alone. A dice roller
with doctored firmware could choose a key it likes *before* the session,
for example by trying keys until action #1 is a natural 20. Fair mode can't
catch that, because no player input goes into the key. It shows that the
rolls weren't changed; it doesn't show the firmware is honest. Use firmware
you built yourself, or trust whoever did.

**How strong is the fingerprint?** It is 48 bits of a SHA-256 hash. To
change a result after the fact, a cheat would need a second key with the
same six words (about 140 trillion tries on average) that also gives the
result they wanted *and* every other roll anyone wrote down. That is far
beyond what anyone would do for a game of dice.

## What each action needs to be checked

An action is checked by typing exactly what was done, with its number.
Most actions stand alone; a few depend on what came before them.

| Action | Type to check it | Needs first |
|---|---|---|
| A roll | `#14 atk +5 vs 15`, `#3 6 x 4d6k3`, `#9 2d6; 1d8` | nothing |
| A table roll | `#7 table forest` | nothing (the same tables) |
| `pct` | `#8 pct b1` | nothing |
| A custom die | `#20 3d{custom}` | `custom 0 ...` with the same faces |
| A reroll | `#15 reroll 2` | the roll it rerolled, typed with its number first |
| A deal | `#22 deal 5` | `deck ...` and every deal since the last shuffle, in order |

The roll must be typed as it was made: `3d6` isn't `3d{custom}`, and
`4d6k3` isn't `4d6`. A line with several rolls separated by `;`, or a
repeat like `6 x 4d6k3`, is one action with one number.

Some commands never get a number, because they don't roll: `deck`,
`shuffle`, `policy`, `custom 0 ...`, `odds`, `stats`, `parse`, `last`.
`odds` and `stats` borrow the generator and put it back, so they can be
used freely during a session. `test` and `next` draw numbers without
taking an action number, which is harmless: the next action starts again
from the key.

## In a program

```c
#include "qd4p.h"

char words[QD_FAIR_TEXT];
qd_fair_start();                                   /* a new secret key */
qd_fair_fingerprint_words(words, sizeof(words));   /* show the six words */

uint32_t n = qd_fair_begin();                      /* before each action */
show_number(n);
qd_roll_run(&roll, &res);

uint8_t key[QD_FAIR_KEY_BYTES];
qd_fair_end(key);                                  /* reveal */
qd_fair_words(key, QD_FAIR_KEY_BYTES, words, sizeof(words));
```

Call `qd_fair_begin()` once per action the players see, right before it
rolls, and show its number with the result. Anything rolled without a
`qd_fair_begin()` carries on from the last action's numbers: still fixed by
the key, but only checkable by replaying that whole action.

To check, `qd_fair_start_key(key)` and `qd_fair_begin_at(n)` before
repeating action `n`. `qd_fair_begin_at()` only works on a session started
from a known key; on a live session it does nothing, so a program can't
redo an action by mistake. `qd_fair_read_words()` turns typed words back
into bytes, accepting any case, commas or hyphens, and three-letter
prefixes. The full API is in `qd4p/qd_fair.h`.

## How it works

For anyone writing a checker of their own (`tests/host/fair_reference.py`
does it in a few lines of Python, with `hashlib`):

- **Key:** 16 random bytes: SHA-256 of fresh hardware randomness and the
  generator's own output (which carries everything stirred in before the
  session), first 16 bytes.
- **Fingerprint:** the first 6 bytes of SHA-256(`"QD4P fair 1 print"` ‖ key).
- **Action n:** SHA-256(`"QD4P fair 1 roll"` ‖ key ‖ n), with n as 4 bytes
  little-endian. Its first 16 bytes, read as four little-endian 32-bit
  words, are the xoshiro128\*\* state s[0..3] (an all-zero state, which
  can't happen in practice, would become s[0] = 0x9E3779B9). The action's
  dice are then drawn exactly as outside fair mode.
- **Words:** each byte is one word from the list in `qd4p/qd_fair.c`: 256
  words of 3 to 6 letters, in alphabetical order (byte 0 is `acorn`, byte
  255 is `zebra`).

The strings are plain ASCII, with no terminating zero. SHA-256 is done in
software in `qd_fair.c` and tested against the standard FIPS 180-4
examples and Python's `hashlib`.
