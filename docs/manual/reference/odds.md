# Odds

[work them out](#qd_odds) · [the answer](#qd_odds_t) · [at least](#qd_odds_at_least) · [exactly](#qd_odds_exactly) · [how](#how-the-odds-are-worked-out)

Header: `qd_odds.h` (included by `qd4p.h`).

**Odds, in three lines:**
1. [`qd_odds`](#qd_odds) works out the chance of every result of a roll, and of passing its target or getting each of its game's outcomes: "you hit 80% of the time, and crit 10%".
2. It is **exact** whenever it can be (most rolls a table uses), and says so when it isn't.
3. It never changes the rolls that come after it: the generator is borrowed and put back, and nothing is counted in [stats for nerds](stats.md).

A guide with more examples, for players: [Part 6, Odds and stats for nerds](../06-odds-and-stats.md).

Odds are one of QD4P's heavier features: about 10 KB of code and 28 KB of working space ([what it costs](../../SIZES.md), and how to shrink it).

---

## qd_odds

Work out the odds of a roll.

```c
qd_err_t qd_odds(const qd_roll_t *r, qd_odds_t *out);
```

```c example=odds
static qd_parsed_t p;
static qd_odds_t odds;
qd_parse_error_t err;
qd_parse("atk adv + 5 vs 15", &p, &err);
qd_odds(&p.roll, &odds);
printf("hit: %.1f%%, critical: %.1f%%, average roll %.2f (%s)\n", 100 * odds.pass,
       100 * odds.level[QD_LVL_CRIT + 4], odds.mean, odds.exact ? "exact" : "approximate");
```

```text output
hit: 79.8%, critical: 9.8%, average roll 18.83 (exact)
```

**Returns** `QD_OK`, or the [error](rolls.md#qd_err_t) that would stop the roll running.

It takes from a fraction of a millisecond (3d6) to about half a second on a Pico 2 (a Savage Worlds roll, simulated). [How](#how-the-odds-are-worked-out) says which rolls take which way.

---

## qd_odds_t

The answer.

```c
typedef struct {
    uint8_t  method;           /* QD_ODDS_EXACT_SUM, _EXACT_ALL, _NORMAL or _SIMULATED */
    bool     exact;
    uint32_t runs;             /* rolls tried (0 for a sum worked out directly) */
    bool     counting;         /* the value is successes, not the total */
    double   mean, sd;         /* the average value, and its spread */
    int32_t  lo, hi;           /* lowest and highest value possible (simulated: seen) */
    bool     judged;           /* a target or game system: pass and level mean something */
    double   pass;             /* the chance of passing */
    double   level[9];         /* the chance of outcome level L at [L + 4] */
    int32_t  first;            /* dist[0] is the chance of the value `first` */
    int32_t  width;            /* values per entry of dist: 1, or more for very wide rolls */
    uint16_t bins;             /* entries in dist */
    float    dist[256];        /* the chance of each value (or group of values) */
    double   below, above;     /* the chance of values outside dist */
} qd_odds_t;
```

"The value" is the total, or the successes for a roll that counts them. For a repeat (`6 x 4d6k3`) the odds are for one roll. `level[]` uses the [outcome levels](systems.md#outcome-levels) of the roll's system; name them with [`qd_sys_level_name`](systems.md#qd_sys_level_name).

```c example=odds_dist
static qd_roll_t roll;
static qd_odds_t odds;
qd_roll_init(&roll);
qd_pool_keep(qd_roll_dice(&roll, 4, 6), QD_KEEP_HIGH, 3);   /* 4d6 k3 */
qd_odds(&roll, &odds);
for (int i = 0; i < odds.bins; i++) {
    printf("%2ld %5.2f%% ", (long)(odds.first + i), 100 * odds.dist[i]);
    for (int x = 0; x < odds.dist[i] * 200; x++) putchar('#');
    putchar('\n');
}
```

```text output
 3  0.08% #
 4  0.31% #
 5  0.77% ##
 6  1.62% ####
 7  2.93% ######
 8  4.78% ##########
 9  7.02% ###############
10  9.41% ###################
11 11.42% #######################
12 12.89% ##########################
13 13.27% ###########################
14 12.35% #########################
15 10.11% #####################
16  7.25% ###############
17  4.17% #########
18  1.62% ####
```

A `qd_odds_t` is 1.2 KB: make it `static`.

---

## qd_odds_at_least

The chance the value is `v` or more.

```c
double qd_odds_at_least(const qd_odds_t *o, int32_t v);
```

```c example=odds_at_least
static qd_parsed_t p;
static qd_odds_t odds;
qd_parse_error_t err;
qd_parse("8d6", &p, &err);                         /* a fireball */
qd_odds(&p.roll, &odds);
for (int hp = 20; hp <= 40; hp += 5)
    printf("kills a %d hp ogre: %4.1f%%\n", hp, 100 * qd_odds_at_least(&odds, hp));
```

```text output
kills a 20 hp ogre: 96.1%
kills a 25 hp ogre: 76.2%
kills a 30 hp ogre: 38.0%
kills a 35 hp ogre:  9.1%
kills a 40 hp ogre:  0.7%
```

**Returns** 0 to 1. Exact when `width` is 1; inside a grouped entry, the values are taken as equally likely.

---

## qd_odds_exactly

The chance the value is exactly `v`.

```c
double qd_odds_exactly(const qd_odds_t *o, int32_t v);
```

```c example=odds_exactly
static qd_parsed_t p;
static qd_odds_t odds;
qd_parse_error_t err;
qd_parse("2d6", &p, &err);
qd_odds(&p.roll, &odds);
printf("a 7: %.4f (1 in %.0f)\n", qd_odds_exactly(&odds, 7), 1 / qd_odds_exactly(&odds, 7));
```

```text output
a 7: 0.1667 (1 in 6)
```

**Returns** 0 to 1 (for a grouped entry, the entry's average per value).

---

## How the odds are worked out

Four ways, the first that fits:

| `method` | For | How |
|---|---|---|
| `QD_ODDS_EXACT_SUM` | sums and success counts of plain dice: no keep, advantage, exploding or rerolls (`8d6`, `3d6 + 1d4 + 2`, `10d10 t8`) | the chances are combined die by die (a convolution). Exact, and fast even for dozens of dice |
| `QD_ODDS_EXACT_ALL` | anything else small enough (`4d6k3`: 1,296 possible rolls; `atk adv`: 400; `coc 65 b2`: 1,000) | every possible roll is tried once, each weighted by its chance. Exact. Up to 50,000 rolls (`QD_ODDS_EXACT_RUNS`) |
| `QD_ODDS_NORMAL` | a plain sum of 20 or more dice, too wide for the first way (`1000d6666`) | the average and spread exactly, and the bell curve for the chances: for that many dice, within a fraction of a percent. Not marked exact |
| `QD_ODDS_SIMULATED` | everything else: dice that explode forever, big rolls with keep | 10,000 rolls (`QD_ODDS_SAMPLES`; fewer for huge ones), counted. Approximate: typically within about 1 percentage point |

The distribution has one entry per value when the roll has up to 256 values (`QD_ODDS_BINS`); wider rolls group values, `width` to an entry, so the whole range fits. [How it works](../10-how-it-works.md#odds) has the details and the maths.
