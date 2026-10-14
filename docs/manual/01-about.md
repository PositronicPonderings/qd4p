# 1. About

QD4P rolls dice, deals cards and works out what a roll means in a game, on a Raspberry Pi Pico 2. `4d6k3`, `atk adv + 5 vs 15`, `sw8 + 1` and `coc 65 b1 vs hard` all mean what a player expects, and the answer comes back as "Critical hit", "Success, 2 raises" or "Hard success", not only a number.

It's written in C for the Pico SDK, and it's built in layers you pay for only if you use them: about 2 KB of flash to roll a die, 12 KB to roll anything built in code, 39 KB to read rolls typed as text. No `malloc`, no surprises at the table: every limit is fixed at build time and every mistake is explained in words.

**What it does:**
- **Dice** of any size, custom dice (loaded, labelled, Fudge), percentile dice with bonus and penalty dice, digit dice (d66).
- **Modifiers:** advantage, exploding, imploding, rerolls, keep and drop, success counting, buckets, matching sets, maths with brackets, targets.
- **Dice notation** read from text, with errors underlined: [Part 5](05-notation.md).
- **Game systems**, with their rules: D&D 5e, Savage Worlds, Call of Cthulhu 7e, FATE, Vampire 5e, Dungeon Crawl Classics.
- **Decks:** poker with jokers, tarot, your own, with reshuffle rules (Savage Worlds' action deck).
- **Rerolls after the roll** (bennies, pushes, willpower, luck), roll sets, and mass rolls of hundreds.
- **Odds** of every result and outcome, exact where possible; **stats for nerds**, with a fairness check.
- **Dice in text** and ranges: the building blocks for random tables, with a complete table system as an example.
- **Fair mode:** rolls the players can check after the session.
- **Honest randomness:** seeded from the RP2350's hardware generator, unbiased, and stirred with anything unpredictable you have, like button presses.

**What it runs on:** the Raspberry Pi Pico 2 and Pico 2 W (RP2350). Everything except the hardware entropy sources is portable C11, built and tested on a PC as well: the original Pico (RP2040) should work, but hasn't been tested.

**What it looks like:**

```c example=about
static qd_parsed_t p;
static qd_result_t res;
qd_parse_error_t err;
qd_outcome_t out;
qd_rng_init_hw();
qd_parse("sw8 + 1 vs 6", &p, &err);
qd_roll_run(&p.roll, &res);
qd_sys_outcome(&p.roll, &res, &out);
printf("%ld: %s (%s)\n", (long)res.total, out.text, out.detail);
```

```text output
14: Success, 2 raises (2 aces, vs 6)
```

**Published by** [Positronic Ponderings](https://github.com/PositronicPonderings).

**Sources:** the dice notation is inspired by [Dice Maiden](https://github.com/Humblemonk/dicemaiden-rs)'s, and not bound to it. The generator is xoshiro128\*\* (Blackman and Vigna) and the unbiased ranges Lemire's method, both from their published descriptions. Game rules are summarised from the games' own rulebooks.

**AI disclosure:** see [`AI_DISCLOSURE.md`](../../AI_DISCLOSURE.md).

**Licence:** MIT-0. Do what you like with it; no credit required.
