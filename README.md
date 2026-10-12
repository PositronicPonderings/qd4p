# QD4P: QuickDice 4 Pico

Dice, cards and game results for the Raspberry Pi Pico 2, in plain C. Roll
`4d6k3`, deal from a deck that knows when to reshuffle, ask "what are the
odds?", and let the library work out raises, success levels and messy
criticals so your program doesn't have to.

Work in progress. Planned layers, each usable on its own:

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

Licence: [MIT-0](LICENSE). How this code was made: [AI_DISCLOSURE.md](AI_DISCLOSURE.md).
