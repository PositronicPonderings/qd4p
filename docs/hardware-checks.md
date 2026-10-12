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

1. **Same rolls as the PC.** Each line starts from `seed 42`:

   | Type | Prints |
   | --- | --- |
   | `custom 20 1 2 3 4 5 6*3` | `4 6 1 5 6 5 6 5 6 6 6 3 6 5 2 3 5 3 5 3` |
   | `d66 10` | `36 14 44 54 55 52 54 13 43 43` |
   | `pct b2 2` | `units 4, tens 90 [00] 50  -> 4` then `units 6, tens [50] 60 50  -> 56` |
   | `pct p1 2` | `units 4, tens [90] 00  -> 94` then `units 5, tens [60] 50  -> 65` |
   | `deck poker 2` then `deal 8` | JD, JkR, 2C, 5H, JH, 8H, KH, 2H |

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
