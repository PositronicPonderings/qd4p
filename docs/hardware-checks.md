# Hardware checks

What to try on a Pico 2 after each milestone, using `qd4p_console`. The PC
tests (`sh tests/host/run_tests.sh`) cover the same ground in more depth;
these confirm the device agrees.

## D0 Skeleton

- `help`, `version`, `limits` respond. Version 0.0.1 at D0.

## D1 Randomness

1. **Same numbers as the PC.** `seed 42` then `next 8` must print exactly:

   ```
   1776835114  4165204688  17111135  2317295270
   2792088233  2554630222  2940343271  2244566231
   ```

   and `seed 42` then `20d6` must print:

   ```
   3 6 1 4 4 4 5 4 5 5 5 2 5 4 1 3 4 3 4 3
   ```

   These come from an independent Python version of the generator
   (`tests/host/rng_reference.py`), so a match means the device, the C code
   and the reference all agree.
2. **Replay ignores stirring.** Still in replay mode: `stir hello` says
   "ignored", `rng` says REPLAY.
3. **Live is different every time.** `live`, then `d 20 10`; power-cycle and
   do it again: different rolls (and different from replay).
4. **Fair dice.** `test 6 600000` and `test 20 600000`: PASS, each face within
   a fraction of a percent. Also reports rolls per second. A fair generator
   fails about 1 run in 1,000; run it again if that happens.
5. **Stirring.** `stir anything`, `trng 32`, `tap 5` (type during the five
   seconds): each reports what it stirred, and `rng` shows the count going up.
6. **ADC (optional).** With something wired to GPIO 26-29 (a photoresistor
   divider or a pot): `adc 28 64` reports 64 readings stirred in.

## D2 Dice and decks

1. **Same rolls as the PC.** Type these in order after one `seed 42` (each
   command carries on from the one before):

   | Type | Prints |
   | --- | --- |
   | `seed 42` | `replay mode, seed 42` |
   | `custom 20 1 2 3 4 5 6*3` | `4 6 1 5 6 5 6 5 6 6 6 3 6 5 2 3 5 3 5 3` |
   | `d66 10` | `22 36 66 43 46 63 42 22 46 15` |
   | `pct b2 2` | `units 6, tens 70 [20] 80  -> 26` then `units 4, tens 50 10 [00]  -> 4` |
   | `pct p1 2` | `units 1, tens [70] 60  -> 71` then `units 9, tens 40 [70]  -> 79` |
   | `deck poker 2` then `deal 8` | 10S, 6D, 10H, QC, 9S, 6H, 3D, 10D |

   (Since D3 the `d66` command is gone: type `dd66` instead. It makes the
   same draws, so `seed 42` then `10dd66` starts 36 14 44.)

2. **Weighted dice are fair to their weights.** `live`, then
   `custom 200000 1 2 3 4 5 6*3`: the six near 37.5%, the rest near 12.5%,
   PASS.
3. **Labels.** `4dF` shows four faces like `[+] [ ] [-] [+]` and a total.
   `custom 10 0/A 0/B 1/C` shows labels instead of values.
4. **Savage Worlds deck.** `deck sw`, then `deal 5` repeatedly. When a joker
   comes up the status line says the next deal reshuffles; the next `deal 5`
   says "(reshuffled ...)" and the deck is back to 49 in it.
5. **Tarot.** `deck tarot`, `deal 3`: full names such as "The Hanged Man" or
   "Knight of Cups".
6. **Reshuffle rules.** `deck poker`, `policy none`, `deal 60`: stops after
   52 with "deck empty". `policy every`, `deal 5` twice: each deal leaves
   only one card out.

## D3 Roll engine

Rolls are typed on their own line (or after `roll`); `notation` lists what
the console understands until the parser arrives in D4. Dice are shown as
`5` kept, `(5)` dropped or set aside, `~2~` rerolled, `!6` from an
explosion (printed right after the die that exploded), `-7` subtracted by
an implosion, `8*` a success, `10**` a double success, `1x` a failure.

1. **Same rolls as the PC.** Type `seed 42` before each line:

   | Type | Prints |
   | --- | --- |
   | `4d6 k3` | `4d6: 3 6 (1) 4  = 13` |
   | `10d6 e6 k8 + 4` | `10d6: (3) 6 !5 (1) (4) 4 4 5 4 5 5  = 38`, `total 42` |
   | `8d10 t8 ie10 f1` | `8d10: 5 10* !9* 1x 6 7 6 7 6  = 57`, `successes 1 (1 failure taken off)` |
   | `2d20 adv1` | `2d20: 9 20 (1)  = 29` |
   | `6d6 r2` | `6d6: 3 6 ~1~ 5 4 4 4  = 26` |
   | `4d10 i1 e10` | `4d10: 5 10 !6 1 -7 6  = 21` |
   | `6d10 t7 ds10` | `6d10: 5 10** 1 6 7* 6  = 35`, `successes 3` |
   | `5dd66 dh1` | `5dd66: 36 14 44 54 (55)  = 148` |
   | `12d4 ie4` | `12d4: 2 4 !4 !3 1 3 3 3 3 3 4 !1 3 3 2  = 42` |
   | `4dF =+` | `4dF: [ ] [+] [-] [ ]  = 0`, `buckets: "+":1 other:3` |

   These come from the Python reference (`tests/host/rng_reference.py`),
   which the PC tests compare against the C engine die by die for 17 kinds
   of roll and 25 seeds.
2. **Summary mode and speed.** `live`, then `1000d6 <3 =6`: the first 100
   dice are listed, then "... 900 more", face counts adding to 1000, an
   average near 3.5, buckets, the summary note and the time taken: a couple
   of milliseconds. `1000d6 k500` sorts all 1000 dice and should still take
   only a few.
3. **Limits are refused, not hung.** `1001d6` says too many dice. `1d6 ie1`
   says it would never stop. `20dd61` rolls (11, 21 ... 61) with a note that
   a one-sided die always shows 1; `20dd60` can't be read (digits are 1-9).
   `4d6 k0` rolls with a note that keeping 0 dice always totals 0.
4. **Maths.** `1d20 - 1d4 * 2` works left to right for now: (d20 - d4) x 2.
   `d%` is a d100.

## D4 Parser

Since D4 the console reads rolls with the real parser: spaces are optional,
maths follows the usual order, and mistakes are underlined. Each roll is
first echoed back in tidy form (what `qd_format` writes), then rolled.
`notation` lists everything briefly; `docs/notation.md` is the full manual.

1. **Same rolls as before.** `seed 42`, then `4d6k3` gives
   `4d6 k3: 3 6 (1) 4  = 13`. `seed 42`, then `10d6e6k8+4` gives total 42,
   as in D3. (The PC tests parse every golden roll as written, without
   spaces, in capitals and as re-written by `qd_format`.)
2. **Precedence.** `1d20 - 1d4 * 2`: the total is the d20 minus twice the
   d4. `(1d8+2)*2` doubles the lot.
3. **Labels, comments, repeats, several rolls.**
   `(Bite) 1d20+14 ! vs AC 15` shows the label and comment.
   `3 x 2d6` rolls three times. `2d6; 1d8+1d4` rolls both.
4. **Errors point at the problem.** Try `4d6 k3 +`, `4 d6`, `(1d6+2`,
   `2d{nope}`, `4d6 k3 k2`, `dd60`: each is underlined with a message.
5. **parse.** `parse (1d8+2)*2 [<3]` shows how it was read (the dice, and
   the sum as a stack: `[dice 1] 2 + 2 *`) without rolling.
6. **Custom dice in rolls.** `custom 0 1 2 3 4 5 6*3` saves a loaded die;
   `3d{custom} k2` rolls it.
7. **Groups.** `base:4d6 t6 + stress:2d6 t6` shows both groups by name and
   the successes.

## D5 Game systems

After the dice, a line starting `=>` gives the game's reading of the roll.
`notation` lists the shortcuts; `docs/notation.md` ("Game systems") has
the rules. Use `live` (or any seed) and try each a few times:

1. **Targets.** `1d20+5 vs 15` (or `v 15`) says Pass or Fail and by how much.
   `6d10 t8 vs 3` judges the successes.
2. **D&D.** `atk adv + 5 vs 15`: the higher d20 counts; a natural 20 says
   Critical hit, a natural 1 Miss whatever the total. `chk - 30 vs 5` can
   never pass, even on a 20 (it notes "natural 20"). `save dis + 2` (or
   `sav`). `chk d20 v 15` is fine; `chk d30 vs 5` says to write `1d30 vs 5`.
3. **Savage Worlds.** `sw8 + 1 vs 6` shows `trait:` and `wild:` dice,
   the better one counting, "Success, N raises" every 4 over, and aces
   (exploded dice) counted. Snake eyes (both 1s) says Critical failure.
   `sw6 x` has no wild die; `sw4-2 x` is an unskilled Extra (d4-2).
4. **Call of Cthulhu.** `coc 65 b2`: the kept result first, the other two
   tens results in brackets, then Regular / Hard / Extreme / Critical /
   Failure / Fumble, and the thresholds (hard 32, extreme 13).
   `coc 40 p1 vs hard` needs 20 or less.
5. **FATE.** `fate + 2` reads the ladder ("Great (+4)"); `fate + 2 vs 3`
   gives shifts: Fail, Tie, Success or Success with style.
6. **Vampire.** `vtm 7 h2 vs 3`: regular and `hunger:` dice; a pair of
   10s is a Critical (Messy if a hunger die is a 10); a failure with a
   hunger 1 is a Bestial failure.
7. **DCC.** `dcc d20 +1d + 2 vs 15` (or `+ d`) rolls a d24 (one step up the chain);
   a natural 24 is a Critical, a 1 a Fumble.
8. **Sets and percentile.** `5d10 sets` lists pairs and triples ("sets:
   2x7"). `d% b2` and `d% p1` work on their own too.
9. **Writing back.** Each roll is echoed in tidy form first: `atk adv+5
   vs 15` comes back as `atk adv + 5 vs 15`.

The PC tests check every system against probabilities worked out exactly
(Savage Worlds success 81.25%, a raise 24.65%, snake eyes 1 in 48; CoC
levels with 0-2 bonus or penalty dice; Vampire criticals and messy
criticals; D&D advantage crits 9.75%) and re-check each outcome against
the dice it came from.

## D6 Rerolls and roll sets

1. **Repeat vs set.** `6 x 4d6k3` rolls six 4d6-keep-3 and ends with a
   `set:` line keeping all six. `(6 x 4d6) k3` rolls six plain 4d6 and the
   `set:` line shows the three lowest in brackets, the total the sum of
   the other three. `(Stats) (7 x 4d6k3) k6` drops exactly one, the lowest.
2. **Whole roll.** `4d6 k3`, then `reroll`: a new roll, and
   "(rerolled 1 time; before: total N)" with the old total.
3. **Chosen dice.** `6d6 t6`, `last` (the dice numbered #1-#6), then
   `reroll 2 3`: those two show old->new (`3->5`), the rest unchanged, and
   successes counted again.
4. **Year Zero push.** `6d6 t6`, then `reroll !=6`: every die that wasn't a
   6 shows old->new, the 6s stay, "N dice rerolled".
5. **Willpower.** `vtm 6 h2 vs 3`, `last`, then `reroll 1 2 3` (regular
   dice): the outcome line is judged again.
6. **Luck.** `1d20`, then `reroll best` (or `worst`): "rolled A and B:
   kept the ..." and the kept roll shown.
7. **Refusals.** `reroll 99` says the die can't be rerolled; `(6x4d6)k7`
   says the set has only 6 rolls.

## D7 Odds and stats for nerds

1. **Exact odds.** `odds 4d6k3`: "exact (all 1296 rolls tried)", average
   12.24, a bar chart from 3 to 18 peaking at 13 (13.27%), 18 at 1.62%.
   `odds 3d6`: "worked out die by die", 10 and 11 at 12.50%.
2. **Outcomes.** `odds atk adv + 5 vs 15`: pass 79.75%, Critical hit 9.75%,
   Hit 70.00%, Miss 20.25%. `odds coc 65 b1`: Critical 1.90%, Extreme
   22.20%, Hard 29.50%, Regular 33.90%, Failure 12.40%, Fumble 0.10%.
3. **Simulated.** `odds sw8 vs 6`: "simulated (10000 rolls)", pass near
   49%. `odds vtm 6 h2 vs 3`: Messy critical and Bestial failure chances.
4. **Big sums.** `odds 100d6`: exact, average 350.00, from 100 to 600, a
   chart of about 20 grouped rows and the 5% / 25% / 50% / 75% / 95%
   points. `odds 1000d6666`: "bell curve", average 3333500.00, instant (it
   took 4.2 s simulated before). `odds 400df`: a chart from about -64 to
   63, not the whole -400 to 400.
5. **Odds don't disturb rolls.** `seed 42`, `odds sw8`, then `next 1`:
   1776835114, the same as without the odds.
6. **Time.** Each odds line ends with how long it took. Nothing should
   take much over a second now (a simulated roll stops at half a million
   dice); `odds 1000d6 ie` is a good worst case.
8. **Width.** Every line fits an 80-column terminal, including the set
   line of `(20x4d6k3)k1`, which wraps.
9. **Mass rolls.** `400 x atk vs 12`: one summary - average, lowest,
   highest, passed, and counts of Critical hit / Hit / Miss adding to 400.
   `1000 x sw6 + 1 vs 6`, `500 x vtm 6 h2 vs 3` likewise. `400 atk vs 12`
   (no x) says to write `400 x`.
7. **Stats.** Roll a few things (`1000d6`, `1000d20`, `4dF`, `dd66`), then
   `stats`: each kind with its rolls, average against fair, top and bottom
   faces and a verdict (`dd66` adds two d6s each time). `stats 20` draws
   the d20's faces. `stats clear` starts again.

## D8 Random tables

The library now has the building blocks (`qd_text.h`); the tables
themselves are the example in `examples/tables/`, which the console uses
for six built-in sample tables (`examples/tables/sample_tables.txt`).

1. **List.** `tables`: Forest encounters (2d6), Bandit leaders (1d4, from
   its ranges), Weather (d6), Treasure (d%), Trinket (dd66), Reaction
   (2d6).
2. **Working.** `table forest` a few times: the 2d6 roll, then any linked
   table indented beneath it (Weather, Bandit leaders), then `=> ` and the
   finished text. A 3-4 shows `{1d4+1}: N` and "N wolves".
3. **Roll twice.** `seed 10`, then `table forest`: a 12, two "again:"
   lines (a 7 and a 6, each with its Weather), and both results joined by
   "; ": "Nothing stirs. Clear skies.; Nothing stirs. Fog rolls in."
4. **Inline maths.** `table treasure`: silver is a multiple of 10 (`{2d6 *
   10}`); a 91-99 links to a Trinket; 00 rolls 3 times.
5. **Names.** `table 6`, `table reac`, `table REACTION` all roll Reaction;
   `table nope` says there's no such table.
6. **On the PC** (optional): `python3 examples/tables/tables.py check
   examples/tables/sample_tables.txt` says OK; `dump` of a built asset
   gives the text back.
