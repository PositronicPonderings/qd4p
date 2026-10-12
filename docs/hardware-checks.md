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

   and `seed 42` then `d 6 20` must print:

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
