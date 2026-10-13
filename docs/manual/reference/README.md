# 4. Reference

Every function, with its exact syntax, what each parameter means, what it returns, and an example that runs. Include `qd4p.h` and you have all of them (the Pico's entropy sources are in `qd_entropy_pico.h`).

Every example in this reference is compiled and run by the test suite, and what it prints is checked against what the page shows, so the pages and the library can't drift apart. The dice come up the same every time in those checks; on a Pico, yours won't.

| Page | Header | What's in it |
|---|---|---|
| [Randomness](randomness.md) | `qd_rng.h`, `qd_entropy_pico.h` | the generator, seeding, stirring in entropy, replay mode, entropy sources |
| [Single dice](dice.md) | `qd_dice.h` | standard, custom (weighted, labelled) and Fudge dice; percentile dice with bonus and penalty dice |
| [Decks of cards](decks.md) | `qd_deck.h` | poker, tarot and your own decks; dealing; reshuffle rules; initiative order |
| [The roll engine](rolls.md) | `qd_roll.h` | building rolls in code, every modifier, running them, results, errors, rerolls after the roll |
| [Notation: the parser](parse.md) | `qd_parse.h` | text to rolls; errors; several rolls; repeats, sets and mass rolls; writing rolls back as text |
| [Game systems](systems.md) | `qd_sys.h` | D&D 5e, Savage Worlds, Call of Cthulhu 7e, FATE, Vampire 5e, DCC: building their rolls and reading their results |
| [Odds](odds.md) | `qd_odds.h` | the chance of every result and outcome |
| [Stats for nerds](stats.md) | `qd_stats.h` | every die tallied; is this die fair? |
| [Dice in text and ranges](text.md) | `qd_text.h` | `{1d4+1} wolves`; rolling on ranges: the building blocks of random tables |
| [Fair mode](fair.md) | `qd_fair.h` | rolls the players can check after the session |
| [Random tables (the example)](tables.md) | `examples/tables/table.h` | a complete table system built on the library: example code, not the library |

Settings, error codes and sizes are in the [appendices](../appendices.md); everything on one page in [the quick reference](../quick-reference.md).
