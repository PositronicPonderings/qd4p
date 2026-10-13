# QD4P: QuickDice 4 Pico

Dice, cards and game results for the Raspberry Pi Pico 2, in plain C. Roll
`4d6k3`, deal from a deck that knows when to reshuffle, ask "what are the
odds?", and let the library work out raises, success levels and messy
criticals so your program doesn't have to.

Work in progress: randomness, dice and decks, the roll engine, the dice
notation parser and the game systems (D&D 5e, Savage Worlds, Call of
Cthulhu 7e, FATE, Vampire 5e, DCC) are done; rerolls and roll sets are
next. The layers, each usable on its own:

| Layer | What it does |
|---|---|
| Randomness | Hardware-seeded generator, unbiased ranges, extra entropy from sensors and knobs, replay mode |
| Dice and decks | Standard, weighted and labelled dice; poker and tarot decks with reshuffle rules |
| Roll engine | Pools, keep/drop, exploding, rerolls, success counting, buckets, big-roll summaries |
| Parser | Dice notation text to a compiled roll, with readable error messages |
| Game systems | D&D 5e, Savage Worlds, Call of Cthulhu 7e, FATE, Vampire 5e, DCC |
| Extras | Rerolls, roll sets, random tables, odds, stats for nerds, fair mode |

Everything except the hardware entropy sources is portable C, built and
tested on a PC as well as the Pico. No dynamic memory. No dependencies
beyond the Pico SDK.

Dice notation: [docs/notation.md](docs/notation.md). Design: [docs/design.md](docs/design.md). What to check on the Pico after each milestone: [docs/hardware-checks.md](docs/hardware-checks.md).

## Build and try it

Pico 2 / Pico 2 W, pico-sdk 2.3.1. Open the folder with the Raspberry Pi
Pico VS Code extension, or:

```
cmake -S . -B build -DPICO_BOARD=pico2      # or pico2_w
cmake --build build
```

Flash `build/examples/qd4p_console.uf2`, open the Pico's USB serial port
(VS Code Serial Monitor, or `screen /dev/ttyACM0 115200`), press Enter and
type `help`.

## Tests on a PC

```
sh tests/host/run_tests.sh
```

Needs a C compiler and python3; no Pico.

## Using QD4P in your project

Copy the `qd4p/` folder, then in your CMakeLists.txt:

```cmake
add_subdirectory(qd4p)
target_link_libraries(my_app pico_stdlib qd4p)
```

and `#include "qd4p.h"`:

```c
qd_parsed_t roll;
qd_parse_error_t err;
qd_result_t res;
if (qd_parse("4d6k3 + 2", &roll, &err))      /* parse once */
    qd_roll_run(&roll.roll, &res);           /* roll as often as you like: res.total, res.dice[] */
```

Rolls can also be built directly in C with `qd_roll.h`, without the parser.

Licence: [MIT-0](LICENSE). How this code was made: [AI_DISCLOSURE.md](AI_DISCLOSURE.md).
