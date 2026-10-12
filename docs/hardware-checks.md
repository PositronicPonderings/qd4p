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

   (`d66` will become the `dd66` digit-dice notation with the parser; see
   the design doc.)

2. **Weighted dice are fair to their weights.** `live`, then
   `custom 200000 1 2 3 4 5 6*3`: the six near 37.5%, the rest near 12.5%,
   PASS.
3. **Labels.** `4dF` shows four faces like `[+][ ][-][+]` and a total.
   `custom 10 0/A 0/B 1/C` shows labels instead of values.
4. **Savage Worlds deck.** `deck sw`, then `deal 5` repeatedly. When a joker
   comes up the status line says the next deal reshuffles; the next `deal 5`
   says "(reshuffled ...)" and the deck is back to 49 in it.
5. **Tarot.** `deck tarot`, `deal 3`: full names such as "The Hanged Man" or
   "Knight of Cups".
6. **Reshuffle rules.** `deck poker`, `policy none`, `deal 60`: stops after
   52 with "deck empty". `policy every`, `deal 5` twice: each deal leaves
   only one card out.
