# 6. Odds and stats for nerds

Two ways QD4P answers "how likely is that?": **odds** work out the chances before you roll, and **stats for nerds** count what actually happened. Odds settle arguments before they start; stats settle the ones about cursed dice.

The C side is in the reference: [odds](reference/odds.md), [stats for nerds](reference/stats.md). The sessions on this page are the console (`qd4p_console`, or `qd4p_pc` on a PC), and the test suite checks that each one prints what's shown.

- [Odds](#odds)
- [Reading the odds](#reading-the-odds)
- [How the odds are worked out](#how-the-odds-are-worked-out)
- [Stats for nerds](#stats-for-nerds)
- [Is my die cursed?](#is-my-die-cursed)

---

## Odds

`odds` and a roll gives the chance of every result:

```console
> odds 2d6
2d6
  exact (worked out die by die), 0 ms
  total: 7.00 on average, spread 2.42, from 2 to 12
                       2  2.78%  #######
                       3  5.56%  #############
                       4  8.33%  ####################
                       5 11.11%  ###########################
                       6 13.89%  #################################
                       7 16.67%  ########################################
                       8 13.89%  #################################
                       9 11.11%  ###########################
                      10  8.33%  ####################
                      11  5.56%  #############
                      12  2.78%  #######
```

With a target or a game system, it also gives the chance of passing, and of each of the game's outcomes:

```console
> odds atk adv + 5 vs 15
atk adv + 5 vs 15
  exact (all 400 rolls tried), 0 ms
  total: 18.83 on average, spread 4.71, from 6 to 25
                       6  0.25%  #
                       7  0.75%  ###
                       8  1.25%  #####
                       9  1.75%  #######
                      10  2.25%  #########
                      11  2.75%  ###########
                      12  3.25%  #############
                      13  3.75%  ###############
                      14  4.25%  #################
                      15  4.75%  ###################
                      16  5.25%  ######################
                      17  5.75%  ########################
                      18  6.25%  ##########################
                      19  6.75%  ############################
                      20  7.25%  ##############################
                      21  7.75%  ################################
                      22  8.25%  ##################################
                      23  8.75%  ####################################
                      24  9.25%  ######################################
                      25  9.75%  ########################################
  pass 79.75%
    Critical hit           9.75%
    Hit                   70.00%
    Miss                  20.25%
```

Every roll works, game systems included:

```console
> odds coc 65 b1 vs hard
coc 65 b1 vs hard
  exact (all 1000 rolls tried), 0 ms
  total: 34.00 on average, spread 23.69, from 1 to 100
                  1 to 5  9.50%  ########################################
                 6 to 10  9.50%  ########################################
                11 to 15  8.50%  ####################################
                16 to 20  8.50%  ####################################
                21 to 25  7.50%  ################################
                26 to 30  7.50%  ################################
                31 to 35  6.50%  ###########################
                36 to 40  6.50%  ###########################
                41 to 45  5.50%  #######################
                46 to 50  5.50%  #######################
                51 to 55  4.50%  ###################
                56 to 60  4.50%  ###################
                61 to 65  3.50%  ###############
                66 to 70  3.50%  ###############
                71 to 75  2.50%  ###########
                76 to 80  2.50%  ###########
                81 to 85  1.50%  ######
                86 to 90  1.50%  ######
                91 to 95  0.50%  ##
               96 to 100  0.50%  ##
  5%: 3  25%: 14  50%: 30  75%: 50  95%: 79
  pass 53.60%
    Critical success       1.90%
    Extreme success       22.20%
    Hard success          29.50%
    Regular success       33.90%
    Failure               12.40%
    Fumble                 0.10%
```

## Reading the odds

- **Average and spread.** The average is what you'd get per roll over a long evening. The spread (the standard deviation) says how far a typical roll strays from it: about two thirds of rolls land within one spread of the average.
- **The bars** are the chance of each result. For a roll with too many possible results to list one by one (`1000d6`), results are grouped, several to a bar.
- **"exact"** means the chances are worked out, not estimated. **"simulated"** means the roll was tried thousands of times and counted: close, typically within a percentage point, but not exact. Exploding dice that can explode forever (Savage Worlds) are always simulated, because there's no end to list.
- **For a roll that counts successes** (`6d10 t8`), the chances are of each number of successes.
- **For a repeat** (`6 x 4d6k3`), the odds are for one roll.

Working out odds never changes the rolls that follow. In replay mode you get exactly the rolls you would have had without asking, and none of the dice are counted in stats.

## How the odds are worked out

QD4P picks the first of four ways that fits:

1. **Exact, die by die.** Sums and success counts of plain dice (no keep, advantage, exploding or rerolls): `3d6`, `1d20 + 5 vs 15`, `6d10 t8 f1`, `100d6`. Each die's chances are combined with the last (a convolution). Fast, even for a hundred dice.
2. **Exact, every roll.** Anything else small enough: every possible roll is tried once, weighted by its chance. `4d6k3` is 1,296 rolls, `atk adv` 400, `coc 65 b1` 1,000. Up to 50,000.
3. **The bell curve.** A plain sum of 20 or more dice too wide for the first way (`1000d6666`): the average and spread are exact, and the chances come from the normal distribution, which for that many dice is within a fraction of a percent.
4. **Simulated.** Everything else: 10,000 rolls, counted.

On a Pico 2: `4d6k3` takes 58 ms, `atk adv` 26 ms, `coc 65 b1` 69 ms, `5d6 k3` 345 ms, `100d6` 34 ms, and a simulated `sw8` about half a second. [How it works](10-how-it-works.md#odds) has the maths.

---

## Stats for nerds

The console counts every die it rolls, from the moment it starts. `stats` lists them:

```console seed=3
> 100 x 1d20 + 2d6
100 x 1d20 + 2d6
  100 rolls: total 17.79 on average, lowest 7, highest 30  (0 ms)
> stats
d20          100 rolls  average 10.83 (fair 10.50)  spread 5.59 (5.77)
          top face 5, bottom 5; chi-square 20.0 (limit 43.8): looks fair
d6           200 rolls  average 3.48 (fair 3.50)  spread 1.66 (1.71)
          top face 28, bottom 33; chi-square 2.1 (limit 20.5): looks fair
```

`stats 20` draws a d20's faces as bars, with the fairness check:

```console seed=3
> 100 x 4d20
100 x 4d20
  100 rolls: total 42.50 on average, lowest 16, highest 69  (0 ms)
> stats 20
d20          400 rolls  average 10.62 (fair 10.50)  spread 5.66 (5.77)
          top face 18, bottom 18; chi-square 16.8 (limit 43.8): looks fair
     1     18  #########################
     2     20  ############################
     3     18  #########################
     4     14  ###################
     5     23  ################################
     6     20  ############################
     7     25  ##################################
     8     15  #####################
     9     23  ################################
    10     26  ####################################
    11     16  ######################
    12     19  ##########################
    13     12  #################
    14     29  ########################################
    15     22  ##############################
    16     21  #############################
    17     22  ##############################
    18     22  ##############################
    19     17  #######################
    20     18  #########################
```

**What counts:** every die the engine rolls, kept or not: dropped, set aside, exploded and rerolled dice are all real rolls of that die. Percentile dice count as the two d10s they are; digit dice (`dd66`) as one die per digit. Dice are kept apart by their sides, and custom dice by which die they are: a loaded d6 is judged as a loaded d6.

`stats clear` starts again.

## Is my die cursed?

The fairness check compares how often each face came up with what a fair die would do (a chi-square test). It needs about 5 rolls per face before it says anything: 30 for a d6, 100 for a d20.

| It says | Means |
|---|---|
| too few rolls | not enough yet to say anything |
| fair | behaving |
| odd | counts this uneven happen to a fair die less than 1 time in 1,000 |

**"Odd" once is a curiosity, not a curse.** A fair die is odd 1 time in 1,000, and over a long campaign with many dice, that happens. The same die odd again and again, after `stats clear`, is worth a look.

To put the device itself to the test, the console's `test` command rolls a die as often as you like, checks the faces the same way, and says how long it took: `test 6 1000000` is a million d6. Curses don't survive sample sizes like that.

A custom die with weights is judged against its own weights: a die loaded to roll sixes three times as often is "fair" if it does exactly that.
