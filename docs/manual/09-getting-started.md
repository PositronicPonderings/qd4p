# 9. Getting started

From a bare Pico to dice rolling over USB. You need a Raspberry Pi Pico 2 or Pico 2 W, a USB cable, and the Pico SDK set up on your computer. For the SDK, the **Raspberry Pi Pico extension for VS Code** is the most direct route; Raspberry Pi's own documentation covers installing it better than we could.

No wiring. QD4P needs nothing but the Pico: the dice come from the RP2350's built-in random number generator. This may be the only Pico project you'll ever start without a breadboard.

[Building](#building) · [Loading a program](#loading-a-program) · [A tour of the console](#a-tour-of-the-console) · [Your first program](#your-first-program) · [CMake in five minutes](#cmake-in-five-minutes) · [Changing a setting](#changing-a-setting) · [On a PC](#on-a-pc) · [Switching programs](#switching-programs)

---

## Building

This repository builds everything at once: the library, the console and the seven examples.

**In VS Code**, with the Pico extension:
1. Open the repository's folder. Since the extension didn't create this project, use its **Import Project** command; it may add its own settings block at the top of `CMakeLists.txt`, which is fine (keep the four SPDX lines first).
2. Pick the board, `pico2` or `pico2_w`, then **Configure CMake** and **Compile Project**.

**From a terminal**, with the SDK installed and `PICO_SDK_PATH` set:

```
cmake -S . -B build -DPICO_BOARD=pico2      # or pico2_w
cmake --build build
```

The programs appear as `.uf2` files: `build/examples/qd4p_console.uf2`, `build/examples/qd4p_hello_dice.uf2` and the rest.

---

## Loading a program

1. Hold the **BOOTSEL** button and plug the Pico in. It appears as a drive called `RP2350`.
2. Drag a `.uf2` file onto it. The drive disappears and the program starts.
3. Open the Pico's **USB serial port**: VS Code's *Serial Monitor* (pick the Pico's port, 115200 baud), or in a terminal `screen /dev/ttyACM0 115200` on Linux, `screen /dev/tty.usbmodem* 115200` on a Mac, or PuTTY on Windows with the COM port Device Manager shows.
4. Press Enter. The console prints its prompt, `>`.

Nothing appears? [Troubleshooting](08-troubleshooting.md#the-console-says-nothing) has the usual suspects.

---

## A tour of the console

`qd4p_console` is everything QD4P does, one command at a time. Type `help` for the list. Any line that isn't a command is tried as a roll, so the quickest start is a roll:

```console seed=1
> 4d6k3
4d6 k3
  4d6 k3: 3 2 (1) 2  = 7
  total 7
> atk adv + 5 vs 15
atk adv + 5 vs 15
  1d20 adv1: (4) 20  = 20
  total 25
  => Critical hit  (natural 20)
> sw8 + 1 vs 6
sw8 + 1 vs 6
  trait:1d8 ie: 8 !6  = 14
  wild:1d6 ie: 4  = 4
  total 15
  => Success, 2 raises  (1 ace, vs 6)
```

Each roll is written back the way it was understood, then the dice (dropped ones in brackets, `!` for a die that exploded), the total, and `=>` the game's reading. [Part 5](05-notation.md) is everything you can type.

**Mistakes are underlined**, with what's wrong:

```console
> 4 d6 + 2
  4 d6 + 2
    ^ no space inside dice: write 4d6
> 4d6 kitten
  4d6 kitten
        ^ t needs a number: t8
```

**The last roll** can be looked at again, with every die numbered, and rerolled: all of it, chosen dice, or dice that pass a test.

```console seed=2
> 5d10 t7
5d10 t7
  5d10 t7: 3 6 2 3 10*  = 24
  total 24, successes 1
> last
5d10 t7
  #1:3 #2:6 #3:2 #4:3 #5:10*
  5d10 t7: 3 6 2 3 10*  = 24
  total 24, successes 1
> reroll 2 4
  5d10 t7: 3 6->4 2 3->7* 10*  = 26
  total 26, successes 2
  (rerolled 1 time; before: total 24)
```

**Odds** before rolling, [Part 6](06-odds-and-stats.md):

```console
> odds 3d6
3d6
  exact (worked out die by die), 0 ms
  total: 10.50 on average, spread 2.96, from 3 to 18
                       3  0.46%  #
                       4  1.39%  ####
                       5  2.78%  #########
                       6  4.63%  ###############
                       7  6.94%  ######################
                       8  9.72%  ###############################
                       9 11.57%  #####################################
                      10 12.50%  ########################################
                      11 12.50%  ########################################
                      12 11.57%  #####################################
                      13  9.72%  ###############################
                      14  6.94%  ######################
                      15  4.63%  ###############
                      16  2.78%  #########
                      17  1.39%  ####
                      18  0.46%  #
```

**Cards and tables:**

```console seed=4
> deck sw
54 cards, 54 in the deck, 0 out; reshuffle: when-empty after-joker
> deal 4
  QC   Queen of Clubs
  6H   Six of Hearts
  JS   Jack of Spades
  JD   Jack of Diamonds
54 cards, 50 in the deck, 4 out; reshuffle: when-empty after-joker
> table forest
Forest encounters
  Forest encounters 2d6: 5
    Weather d6: 4
  => Nothing stirs. Clear skies.
```

The rest of the commands:

| Command | Does |
|---|---|
| `parse 4d6k3` | how a roll is read, without rolling it |
| `notation` | a summary of the notation |
| `stats`, `stats 20`, `stats clear` | [stats for nerds](06-odds-and-stats.md#stats-for-nerds) |
| `pct b2` | a percentile roll with Call of Cthulhu bonus dice, showing every die |
| `custom 0 1 2 3 4 5 6*3` | define a die (here, a six three times as likely): then roll it as `3d{custom}` |
| `deck poker 2`, `deck tarot`, `deal 5`, `shuffle`, `policy joker empty` | [decks](reference/decks.md) |
| `tables`, `table 4` | the sample [random tables](reference/tables.md) |
| `fair`, `fair end`, `fair check ...` | [fair mode](07-fair-mode.md) |
| `seed 42`, `live` | replay mode on (the same rolls every time) and off |
| `stir any text`, `trng`, `adc 28`, `tap` | stir in more randomness: typed text, the hardware generator, a pin's noise, your key timings |
| `test 6 100000`, `next` | a fairness test of the generator; raw 32-bit numbers |
| `rng`, `version`, `limits` | the generator's state, the version, the compile-time limits |

---

## Your first program

The smallest program that rolls dice, with everything in view:

```c example=first_program compile-only
#include <stdio.h>
#include "pico/stdlib.h"
#include "qd4p.h"

int main(void)
{
    stdio_init_all();                  /* printf() over USB serial */
    qd_rng_init_hw();                  /* seed the dice from the hardware, once */

    static qd_roll_t roll;             /* static: these are too big for the stack */
    static qd_result_t res;
    qd_roll_init(&roll);
    qd_pool_keep(qd_roll_dice(&roll, 4, 6), QD_KEEP_HIGH, 3);    /* 4d6, keep 3 */

    for (;;) {
        qd_roll_run(&roll, &res);
        printf("4d6 k3: %ld\n", (long)res.total);
        sleep_ms(1000);
    }
}
```

Three steps, and they're the same in every program: **seed** once at boot, **build** the roll once, **run** it whenever a roll is wanted. [Part 3](03-examples.md) has seven programs that do more; [Quick answers](02-quick-answers.md) finds the function for whatever you want to do next.

---

## CMake in five minutes

CMake reads `CMakeLists.txt` and works out how to build your program. A Pico project with QD4P needs about ten lines, and every one of them matters:

```cmake
cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)          # find the Pico SDK (the file comes with it)
project(my_project C CXX ASM)           # a name for the whole project
pico_sdk_init()                         # set up the SDK

add_subdirectory(qd4p)                  # build the library (the folder you copied in)

add_executable(my_app main.c)           # your program: its name, and EVERY .c file in it
target_link_libraries(my_app            # what it uses:
    pico_stdlib                         #   the Pico basics
    qd4p)                               #   QD4P
pico_enable_stdio_usb(my_app 1)         # printf() goes to USB serial
pico_add_extra_outputs(my_app)          # make my_app.uf2 as well
```

**To use QD4P in your project**, copy the `qd4p/` folder into it and add the lines above. That folder is the whole library; nothing else in this repository is needed.

The things that go wrong, and why:
- **The program's name must be the same everywhere** it appears: `add_executable(my_app ...)`, `target_link_libraries(my_app ...)`, and so on. A mismatch gives "not built by this project", which is CMake's way of saying you've introduced it to a stranger.
- **Every `.c` file must be listed** in `add_executable`. A missing one gives "undefined reference" when linking, naming something from that file.
- **After adding a file or a program, configure again** (*Configure CMake*). CMake only reads `CMakeLists.txt` when configuring.
- Several programs in one project are several `add_executable` blocks, each with its own name. That's how this repository builds eight at once (`examples/CMakeLists.txt` loops over them).

---

## Changing a setting

Every limit, from the most dice in a roll to the size of the odds tables, is a setting in [`qd4p/qd_config.h`](../../qd4p/qd_config.h) ([the appendix](appendices.md#settings) lists them). Change one from your `CMakeLists.txt`, on the library and marked `PUBLIC`, so the library and your program agree on its value:

```cmake
target_compile_definitions(qd4p PUBLIC QD_MAX_ROLL_DICE=100 QD_MAX_KEPT_DICE=50)
```

Those two take the roll engine's RAM from 16 KB to under 2 KB: [what QD4P costs](../SIZES.md) says what else is worth changing.

---

## On a PC

Everything except the hardware entropy sources is portable C, so QD4P also builds and runs on a PC: Linux, macOS, or Windows with WSL.

**The tests:**

```
sh tests/host/run_tests.sh
```

They need a C compiler and python3. They test the library, check every example in this manual compiles, runs and prints what the page shows, and check every roll in the notation guide.

**The console on a PC:** the same `console.c`, built with stand-ins for the Pico's USB functions. Its main job is [checking a fair-mode session](07-fair-mode.md#checking-afterwards), but everything works:

```
sh examples/pc/build.sh
examples/pc/qd4p_pc
```

---

## Switching programs

Holding BOOTSEL and replugging for every test gets old fast. Once **any** program with USB serial is running (all of these have it), `picotool` can reboot it into BOOTSEL over USB and load the next:

```
picotool load -f -x build/examples/qd4p_dice_box.uf2
```

`-f` forces the reboot; `-x` starts the new program. The button is only needed when the Pico is running something without USB serial, or is brand new.
