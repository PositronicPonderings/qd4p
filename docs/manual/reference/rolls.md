# The roll engine

[how a roll runs](#how-a-roll-runs) · [tests](#qd_test_t) · [init](#qd_roll_init) · [dice](#qd_roll_dice) · [minus dice](#qd_roll_dice_minus) · [custom](#qd_roll_custom) · [percentile](#qd_roll_percent) · [digit dice](#qd_roll_digits) · [maths](#qd_roll_add) · [buckets](#qd_roll_bucket) · [advantage](#qd_pool_advantage) · [explode](#qd_pool_explode) · [implode](#qd_pool_implode) · [reroll](#qd_pool_reroll) · [keep / drop](#qd_pool_keep) · [counting](#qd_pool_success) · [sets, names, hidden](#other-pool-settings) · [run](#qd_roll_run) · [check](#qd_roll_check) · [results](#qd_result_t) · [errors](#qd_err_t) · [warnings](#qd_roll_warnings) · [reroll all](#qd_reroll_all) · [reroll dice](#qd_reroll_dice) · [reroll matching](#qd_reroll_matching) · [better of two](#qd_reroll_keep) · [observer](#qd_roll_observe) · [pool range](#qd_pool_range) · [low level](#qd_roll_new_pool)

Header: `qd_roll.h` (included by `qd4p.h`).

**The roll engine, in four lines:**
1. A **roll** (`qd_roll_t`) is one or more **pools** (dice of one kind and what to do with them) and a little **sum** that combines the pools' totals with numbers: `3d6 + 1d4 + 2`.
2. You **build** a roll once, with the functions on this page or from notation with [the parser](parse.md), and **run** it as often as you like: every run is a fresh roll.
3. Running fills in a **result** (`qd_result_t`): the total, every die with what happened to it, successes, counts.
4. Nothing here needs the parser. An app that builds its rolls in code doesn't carry the parser's 15 KB ([what it costs](../../SIZES.md)).

```c example=rolls_intro
qd_roll_t roll;
qd_result_t res;
qd_rng_init_hw();

qd_roll_init(&roll);                               /* 4d6, keep the best 3 */
qd_pool_keep(qd_roll_dice(&roll, 4, 6), QD_KEEP_HIGH, 3);
for (int i = 0; i < 6; i++) {
    qd_roll_run(&roll, &res);
    printf("%ld ", (long)res.total);
}
printf("\n");
```

```text output
16 15 12 13 16 11
```

`qd_roll_t` is 1.4 KB and `qd_result_t` 2.2 KB: make them `static` or global rather than local, since the Pico's stack is only 2 KB.

---

## How a roll runs

Whatever order the modifiers were added in, a roll always runs in the same stages. Most surprises about dice come from not knowing this order, so here it is:

1. **Roll** each pool, plus one extra die per level of advantage or disadvantage; set aside that many lowest (advantage) or highest (disadvantage) dice. Set-aside dice never explode.
2. **Explode, implode and reroll**, in the order they were added. Each step looks at every die still in play.
3. **Keep or drop.** This can keep or drop a die that came from an explosion.
4. **Pool total**: the kept dice, minus any imploded dice.
5. **The sum**: pool totals and numbers, `+ - * /`.
6. **Counting** (successes, failures, botches) and buckets, over the kept dice. Imploded dice are never counted.

**Ties.** When dice tie, "lowest" means the earliest-rolled of them and "highest" the latest-rolled. Totals don't depend on this; which die is shown as dropped does, and this rule makes it the same every time.

The same order, with examples in notation: [the notation guide](../05-notation.md#the-order-things-happen-in).

---

## qd_test_t

A **test** says which faces something applies to: explode on a 6, count 8 or more as a success, reroll 1s. Tests are small values, passed around by value.

```c
qd_test_t qd_test_ge(int a);          /* a or more */
qd_test_t qd_test_le(int a);          /* a or less */
qd_test_t qd_test_gt(int a);          /* more than a */
qd_test_t qd_test_lt(int a);          /* less than a */
qd_test_t qd_test_eq(int a);          /* exactly a */
qd_test_t qd_test_ne(int a);          /* anything but a */
qd_test_t qd_test_range(int a, int b);/* a to b */
qd_test_t qd_test_max(void);          /* the die's highest face: a natural 20 on a d20 */
qd_test_t qd_test_min(void);          /* the die's lowest face */
qd_test_t qd_test_label(const char *label);   /* a custom die's face with this label */

bool  qd_test_face(const qd_test_t *t, int16_t value, const char *label, int16_t lo, int16_t hi);
char *qd_test_text(const qd_test_t *t, char *buf, int size);
```

```c example=test_face
qd_test_t tests[] = { qd_test_ge(5), qd_test_range(2, 3), qd_test_max(), qd_test_ne(6) };
char t[16];
for (int k = 0; k < 4; k++) {
    printf("%-5s:", qd_test_text(&tests[k], t, sizeof t));
    for (int v = 1; v <= 6; v++)
        if (qd_test_face(&tests[k], (int16_t)v, "", 1, 6)) printf(" %d", v);
    printf("\n");
}
```

```text output
>=5  : 5 6
2-3  : 2 3
=max : 6
!=6  : 1 2 3 4 5
```

- `qd_test_face` asks whether a face passes. `label` may be `""`; `lo` and `hi` are the die's lowest and highest values, which `max` and `min` need.
- `qd_test_text` writes the test the way notation would: `>=5`, `2-3`, `=max`. It returns `buf`.
- `qd_test_max` is better than `qd_test_ge(20)` for "a natural 20" when the die might change: on a d12 it means 12.

---

## qd_roll_init

Start an empty roll.

```c
void qd_roll_init(qd_roll_t *r);
```

Call it before building, and to start again with the same `qd_roll_t`. The [game-system builders](systems.md) call it for you.

---

## qd_roll_dice

Add a pool of standard dice: `count` dice of `sides` sides, added to the total.

```c
qd_pool_t *qd_roll_dice(qd_roll_t *r, uint16_t count, uint16_t sides);
```

```c example=roll_dice
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_dice(&roll, 3, 6);                  /* 3d6 */
qd_roll_dice(&roll, 1, 4);                  /* + 1d4 */
qd_roll_add(&roll, 2);                      /* + 2 */
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++)
    printf("d%d:%d ", roll.pools[res.dice[i].pool].sides, res.dice[i].value);
printf(" + 2 = %ld\n", (long)res.total);
```

```text output
d6:6 d6:4 d6:6 d4:1  + 2 = 19
```

| Parameter | Meaning |
|---|---|
| `count` | 1 to 1,000 (`QD_MAX_ROLL_DICE`) |
| `sides` | 1 to 32,767 |

**Returns** the new pool, to add modifiers to with the `qd_pool_*` functions, or `NULL` if the roll already has 8 pools (`QD_MAX_POOLS`). A `NULL` also marks the roll, so [`qd_roll_run`](#qd_roll_run) refuses it with `QD_ERR_FULL` instead of rolling something different from what you built. The `qd_pool_*` functions accept `NULL` and do nothing, so chained calls are safe.

---

## qd_roll_dice_minus

A pool whose total is **subtracted**: `1d20 - 1d4`.

```c
qd_pool_t *qd_roll_dice_minus(qd_roll_t *r, uint16_t count, uint16_t sides);
```

```c example=roll_dice_minus
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_dice(&roll, 1, 20);
qd_roll_dice_minus(&roll, 1, 4);            /* bane: -1d4 */
qd_roll_run(&roll, &res);
printf("%d - %d = %ld\n", res.dice[0].value, res.dice[1].value, (long)res.total);
```

```text output
19 - 3 = 16
```

---

## qd_roll_custom

A pool of custom dice: loaded, labelled, Fudge.

```c
qd_pool_t *qd_roll_custom(qd_roll_t *r, uint16_t count, const qd_die_t *die);
```

```c example=roll_custom
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_custom(&roll, 4, qd_fudge_die());   /* 4dF */
qd_roll_add(&roll, 2);                      /* + Fair (+2) */
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++)
    printf("[%s]", qd_die_face_label(qd_fudge_die(), res.dice[i].face));
printf(" + 2 = %+ld\n", (long)res.total);
```

```text output
[+][ ][+][-] + 2 = +3
```

| Parameter | Meaning |
|---|---|
| `die` | the die ([single dice](dice.md)). **Not copied**: it must stay alive for as long as the roll is used |

---

## qd_roll_percent

A pool of percentile dice (d%): a units d10 and a tens d10, read 1 to 100, with Call of Cthulhu bonus and penalty dice.

```c
qd_pool_t *qd_roll_percent(qd_roll_t *r, uint16_t count, int bonus);
```

```c example=roll_percent
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_percent(&roll, 1, -1);              /* one penalty die: keep the worse tens */
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++)    /* the result, then the one not used */
    printf(res.dice[i].flags & QD_DIE_SET_ASIDE ? "(%d) " : "%d ", res.dice[i].value);
printf("= %ld\n", (long)res.total);
```

```text output
99 (59) = 99
```

| Parameter | Meaning |
|---|---|
| `count` | how many d100 rolls in the pool |
| `bonus` | bonus (+) or penalty (-) dice, -3 to +3 |

The tens dice that weren't used are listed in the result, set aside, as the results they would have given. Percentile pools can't explode, reroll or take advantage (bonus dice are their advantage); [`qd_roll_run`](#qd_roll_run) refuses that with `QD_ERR_PERCENT`. For a skill check with success levels, [`qd_sys_coc`](systems.md#qd_sys_coc) does the reading too.

---

## qd_roll_digits

A pool of digit dice: each die is read as a digit. `"66"` is a tens d6 and a units d6, read as 11 to 66 (Warhammer, Traveller, Mothership tables).

```c
qd_pool_t *qd_roll_digits(qd_roll_t *r, uint16_t count, const char *digits);
```

```c example=roll_digits
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_digits(&roll, 3, "66");             /* three d66 rolls */
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++) printf("%d ", res.dice[i].value);
printf("\n");
```

```text output
64 62 46
```

| Parameter | Meaning |
|---|---|
| `digits` | one character per digit, each 1 to 9: the sides of that digit's die. `"666"` reads 111 to 666, `"36"` a d3 then a d6. Up to 4 digits (`QD_MAX_DIGITS`) |

A digit with a 1-sided die always shows 1; that's allowed, with a warning (`QD_WARN_ONE_SIDED`). Digit dice can be kept, dropped and counted, but not exploded, imploded or rerolled: `QD_ERR_DIGITS`.

---

## qd_roll_add

Arithmetic on the running total, in the order you add it: + n, × n, ÷ n.

```c
void qd_roll_add(qd_roll_t *r, int32_t n);       /* n may be negative */
void qd_roll_mul(qd_roll_t *r, int32_t n);
void qd_roll_div(qd_roll_t *r, int32_t n);
```

```c example=roll_add
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_dice(&roll, 2, 6);
qd_roll_mul(&roll, 10);                     /* 2d6 x 10 gold */
qd_roll_run(&roll, &res);
printf("%d + %d, x 10 = %ld gp\n", res.dice[0].value, res.dice[1].value, (long)res.total);
```

```text output
6 + 4, x 10 = 100 gp
```

Each step applies to everything before it: dice, then `+ 2`, then `* 2` is (dice + 2) × 2. For the usual precedence and brackets, write notation and let [the parser](parse.md) build it, or push the sum yourself ([low level](#qd_roll_new_pool)).

Division rounds toward zero, as C does: 7 / 2 is 3, -7 / 2 is -3. Dividing by the number 0 is refused when the roll is built (`QD_ERR_DIV_ZERO`); a total that turns into 0 on the way gives 0 for that step, and the result is flagged `QD_RES_DIV_ZERO`.

---

## qd_roll_bucket

Count the kept dice that pass a test, in **buckets**: how many 1s, how many 6s, how many in between.

```c
bool qd_roll_bucket(qd_roll_t *r, qd_test_t t);
```

```c example=roll_bucket
static qd_roll_t roll;
static qd_result_t res;
char t[16];
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_dice(&roll, 30, 6);                 /* a horde's 30 attacks */
qd_roll_bucket(&roll, qd_test_eq(6));
qd_roll_bucket(&roll, qd_test_range(4, 5));
qd_roll_run(&roll, &res);
for (int b = 0; b < res.bucket_count; b++)
    printf("%s: %u  ", qd_test_text(&roll.buckets[b].test, t, sizeof t), res.buckets[b]);
printf("other: %u\n", res.buckets[res.bucket_count]);
```

```text output
=6: 9  4-5: 9  other: 12
```

**Returns** `false` if the roll already has 8 buckets (`QD_MAX_BUCKETS`). A die can land in more than one bucket; dice in none are counted in one more, "everything else", at `res.buckets[res.bucket_count]`.

---

## qd_pool_advantage

Advantage (positive) or disadvantage (negative): roll that many extra dice, and set aside that many of the lowest or highest, **before** anything explodes.

```c
void qd_pool_advantage(qd_pool_t *p, int n);
```

```c example=pool_advantage
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_advantage(qd_roll_dice(&roll, 1, 20), 1);     /* 1d20 with advantage */
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++)
    printf(res.dice[i].flags & QD_DIE_SET_ASIDE ? "(%d) " : "%d ", res.dice[i].value);
printf("= %ld\n", (long)res.total);
```

```text output
19 (11) = 19
```

Advantage and disadvantage cancel, so pass the difference: two levels of advantage and one of disadvantage is `1`. Calling it again replaces the level. Because the extra dice are set aside before exploding, a set-aside die never explodes, and the dice explosions add are never set aside. Compare [keep / drop](#qd_pool_keep), which comes after exploding. For D&D's advantage on an attack, [`qd_sys_dnd`](systems.md#qd_sys_dnd) does all of it.

---

## qd_pool_explode

**Exploding** dice: a die that passes the test rolls one more die, which joins the pool.

```c
bool qd_pool_explode(qd_pool_t *p, qd_test_t when, bool forever);
```

```c example=pool_explode
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_explode(qd_roll_dice(&roll, 6, 6), qd_test_max(), true);   /* 6d6, 6s explode */
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++)
    printf(res.dice[i].flags & QD_DIE_EXPLODED ? "!%d " : "%d ", res.dice[i].value);
printf("= %ld (%u dice)\n", (long)res.total, res.rolled);
```

```text output
6 4 6 2 4 6 !2 !5 !2 = 37 (9 dice)
```

| Parameter | Meaning |
|---|---|
| `when` | which faces explode: `qd_test_max()` for "on the highest face", `qd_test_ge(9)` for 9 or more |
| `forever` | `false`: the new die doesn't explode (`e`). `true`: it can, and so can its dice (`ie`), up to 100 extra dice per die (`QD_MAX_EXPLOSIONS`) |

**Returns** `false` if the pool already has 4 explode, implode and reroll steps (`QD_MAX_POOL_STEPS`).

In the result, a die from an explosion has `QD_DIE_EXPLODED` and its `parent` is the die that exploded. A chain stopped by the limit is flagged `QD_DIE_CAPPED` and `QD_RES_CAPPED`. An `ie` that every face passes (`1d6 ie1`) would never stop, so it's refused: `QD_ERR_FOREVER`.

---

## qd_pool_implode

**Imploding** dice: a die that passes the test rolls one more die, which is **subtracted** from the total. Cyberpunk RED's critical failure.

```c
bool qd_pool_implode(qd_pool_t *p, qd_test_t when);
```

```c example=pool_implode
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_t *p = qd_roll_dice(&roll, 1, 10);
qd_pool_implode(p, qd_test_eq(1));           /* a 1: roll again and subtract */
qd_pool_explode(p, qd_test_eq(10), false);   /* a 10: roll again and add */
for (int n = 0; n < 8; n++) {
    qd_roll_run(&roll, &res);
    printf("%ld ", (long)res.total);
}
printf("\n");
```

```text output
16 13 7 13 8 4 4 7
```

An imploded die never joins the pool: it can't be kept, dropped or counted. **Returns** `false` if the pool has no room for another step.

---

## qd_pool_reroll

**Rerolls**: a die that passes the test is set aside and rolled again, once (`r`) or until it stops passing (`ir`).

```c
bool qd_pool_reroll(qd_pool_t *p, qd_test_t when, bool forever);
```

```c example=pool_reroll
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_reroll(qd_roll_dice(&roll, 2, 6), qd_test_le(2), false);   /* Great Weapon Fighting */
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++)
    printf(res.dice[i].flags & QD_DIE_REROLLED ? "~%d~ " : "%d ", res.dice[i].value);
printf("= %ld\n", (long)res.total);
```

```text output
6 4 = 10
```

In the result the first roll is kept, flagged `QD_DIE_REROLLED` (and out of play), and the new die is flagged `QD_DIE_REPLACEMENT`. **Returns** `false` if the pool has no room.

For rerolling **after** the roll (a benny, a push): [`qd_reroll_all`](#qd_reroll_all) and friends.

---

## qd_pool_keep

**Keep** or **drop** some of the dice, after exploding: keep the best 3, drop the lowest.

```c
void qd_pool_keep(qd_pool_t *p, qd_keep_t how, uint16_t n);
```

| `how` | Notation | Means |
|---|---|---|
| `QD_KEEP_HIGH` | `k3` | keep the `n` highest |
| `QD_KEEP_LOW` | `kl3` | keep the `n` lowest |
| `QD_KEEP_MIDDLE` | `km3` | keep the `n` in the middle |
| `QD_DROP_LOW` | `d1` | drop the `n` lowest |
| `QD_DROP_HIGH` | `dh1` | drop the `n` highest |
| `QD_KEEP_ALL` | | the default: keep everything |

```c example=pool_keep
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_keep(qd_roll_dice(&roll, 5, 10), QD_KEEP_MIDDLE, 3);
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++)
    printf(res.dice[i].flags & QD_DIE_DROPPED ? "(%d) " : "%d ", res.dice[i].value);
printf("= %ld\n", (long)res.total);
```

```text output
10 6 (10) (3) 7 = 23
```

Keeping more dice than there are keeps them all. Keeping 0 is allowed (the total is 0), with the warning `QD_WARN_KEEP_NONE`. With middle, when the dropped dice can't split evenly, the extra one is dropped from the bottom.

---

## qd_pool_success

**Counting**, instead of adding: each kept die that passes a test is a success. `twice` makes some dice worth two, `failure` takes one away, and botches are counted on their own.

```c
void qd_pool_success(qd_pool_t *p, qd_test_t t);   /* each passing die: +1 success */
void qd_pool_twice(qd_pool_t *p, qd_test_t t);     /* ... and +1 more if it passes this too */
void qd_pool_failure(qd_pool_t *p, qd_test_t t);   /* each passing die: -1 success */
void qd_pool_botch(qd_pool_t *p, qd_test_t t);     /* counted separately */
```

```c example=pool_success
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_t *p = qd_roll_dice(&roll, 8, 10);         /* Exalted: 7+ succeeds, 10s twice */
qd_pool_success(p, qd_test_ge(7));
qd_pool_twice(p, qd_test_eq(10));
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++)
    printf(res.dice[i].flags & QD_DIE_TWICE ? "%d** " : res.dice[i].flags & QD_DIE_SUCCESS ? "%d* " : "%d ",
           res.dice[i].value);
printf(": %d successes\n", res.successes);
```

```text output
10** 6 10** 3 7* 10** 3 8* : 8 successes
```

A roll with any counting pool sets `QD_RES_COUNTING` in its result: show `res.successes` (successes minus failures), not `res.total`. Each pool's own counts are in `res.pools[k]`. Counting looks at the kept dice only, so it works with keep and drop.

---

## Other pool settings

A few settings are plain fields of the pool:

```c
qd_pool_t *p = qd_roll_dice(&roll, 5, 10);
p->sets = true;                            /* report matching sets: pairs, triples ... */
strcpy(p->name, "hunger");                 /* a group name, up to 7 characters */
p->hidden = true;                          /* yours to use: e.g. shown only to the DM */
```

```c example=pool_sets
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_t *p = qd_roll_dice(&roll, 10, 10);         /* One-Roll Engine: look for matches */
p->sets = true;
qd_roll_run(&roll, &res);
for (int i = 0; i < res.dice_count; i++) printf("%d ", res.dice[i].value);
printf("\nsets:");
for (int s = 0; s < res.pools[0].set_count; s++)
    printf(" %u x %d", res.pools[0].sets[s].size, res.pools[0].sets[s].value);
printf("\n");
```

```text output
10 6 10 3 7 10 3 8 4 4
sets: 3 x 10 2 x 4 2 x 3
```

Sets are listed biggest first, then highest value first, up to 8 (`QD_MAX_SETS`). The engine doesn't use `name` or `hidden`; the parser fills in `name` from `hunger:5d10`, and the game systems use names to tell their dice apart.

---

## qd_roll_run

Roll it.

```c
qd_err_t qd_roll_run(const qd_roll_t *r, qd_result_t *res);
```

**Returns** `QD_OK`, or an [error](#qd_err_t): then `res` is cleared and nothing is rolled. A roll that ran fills in everything in [`qd_result_t`](#qd_result_t). Examples: everywhere on this page.

A roll is never changed by running it, so build it once and run it every time the button is pressed. The engine has one shared working buffer: run rolls from one core at a time.

---

## qd_roll_check

Check a roll without rolling it: what [`qd_roll_run`](#qd_roll_run) would refuse.

```c
qd_err_t qd_roll_check(const qd_roll_t *r);
```

```c example=roll_check
static qd_roll_t roll;
qd_roll_init(&roll);
qd_pool_explode(qd_roll_dice(&roll, 1, 6), qd_test_ge(1), true);   /* every face explodes, forever */
printf("%s\n", qd_err_text(qd_roll_check(&roll)));
```

```text output
explodes or rerolls on every face, so it would never stop
```

---

## qd_result_t

What a run gives you.

```c
typedef struct {
    int32_t  total;            /* the sum */
    int16_t  successes;        /* every pool: successes minus failures */
    int16_t  failures, botches;
    uint16_t flags;            /* QD_RES_*, below */
    uint16_t warnings;         /* QD_WARN_*: rolled, but worth a note */
    uint16_t rolled;           /* every die rolled, set aside and rerolled included */
    uint8_t  rerolls;          /* times rerolled after the fact */
    int32_t  was_total;        /* after a reroll: the total before it */
    int16_t  was_successes;
    uint8_t  pool_count;
    qd_pool_result_t pools[8]; /* each pool's share, below */
    uint8_t  bucket_count;
    uint16_t buckets[9];       /* each bucket's count; [bucket_count] is everything else */
    uint16_t dice_count;       /* dice listed in dice[] */
    qd_die_result_t dice[100]; /* every die, in pool order (QD_MAX_KEPT_DICE) */
} qd_result_t;
```

**Each die** (`qd_die_result_t`): `value`, `face` (the face index; for digit dice the value), `pool` (which pool it's in), `parent` (the die that exploded, imploded or was rerolled into this one, or `QD_NO_PARENT`), `was` (its first value, if rerolled after the fact) and `flags`:

| Flag | The die... |
|---|---|
| `QD_DIE_DROPPED` | wasn't kept (keep or drop) |
| `QD_DIE_SET_ASIDE` | was set aside by advantage or disadvantage, or is an unused percentile tens die |
| `QD_DIE_REROLLED` | was rerolled: the new die replaces it |
| `QD_DIE_EXPLODED` | came from an explosion |
| `QD_DIE_IMPLODED` | came from an implosion: subtracted, never counted |
| `QD_DIE_REPLACEMENT` | came from a reroll |
| `QD_DIE_MAX`, `QD_DIE_MIN` | shows the die's highest or lowest face: a natural 20, a natural 1 |
| `QD_DIE_SUCCESS`, `QD_DIE_TWICE`, `QD_DIE_FAILURE`, `QD_DIE_BOTCH` | counted |
| `QD_DIE_CAPPED` | its explosion chain hit the limit |
| `QD_DIE_CHANGED` | was rerolled after the fact: `was` holds what it first showed |

`QD_DIE_OUT` is the dice not in play: dropped, set aside, rerolled or imploded. They don't count toward the total, successes or buckets.

**Each pool** (`qd_pool_result_t`): `total` (kept dice minus imploded), `rolled`, `kept`, `min` and `max` (of the kept dice), `successes`, `failures`, `botches`, where its dice start in `dice[]` (`first`) and how many are listed (`listed`), matching `sets`, and, when it rolled more dice than are listed, `face_count[]` (kept dice per face, for dice of up to 30 faces).

**Result flags:**

| Flag | Means |
|---|---|
| `QD_RES_SUMMARY` | more than 100 dice: only the first 100 are listed. Every number (total, counts, min, max, face counts) is still exact |
| `QD_RES_CAPPED` | an explosion or reroll chain was cut short by the limit |
| `QD_RES_DIV_ZERO` | the sum divided by zero; that step gave 0 |
| `QD_RES_CLAMPED` | the total didn't fit in 32 bits |
| `QD_RES_COUNTING` | a pool counts successes: show `successes`, not `total` |
| `QD_RES_REROLLED` | rerolled after the fact: see `was_total` |
| `QD_RES_KEPT_FIRST` | better or worse of two: the first roll was kept |

```c example=result_summary
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_dice(&roll, 1000, 6);               /* a big one */
qd_roll_run(&roll, &res);
const qd_pool_result_t *p = &res.pools[0];
printf("total %ld from %u dice, %u listed%s\n", (long)res.total, p->rolled, p->listed,
       res.flags & QD_RES_SUMMARY ? " (summary)" : "");
printf("sixes: %u, average %.2f\n", p->face_count[5], (double)p->total / p->kept);
```

```text output
total 3513 from 1000 dice, 100 listed (summary)
sixes: 155, average 3.51
```

---

## qd_err_t

What can go wrong when building or running a roll, and its text for a person.

```c
const char *qd_err_text(qd_err_t e);
```

| Error | Means |
|---|---|
| `QD_OK` | it worked |
| `QD_ERR_FULL` | a builder ran out of room: pools, steps, sum, buckets |
| `QD_ERR_NO_DICE` | a pool with 0 dice, or a 0-sided die |
| `QD_ERR_TOO_MANY_DICE` | more than 1,000 dice (`QD_MAX_ROLL_DICE`) before exploding |
| `QD_ERR_DIGITS` | bad digit dice, or exploding or rerolling them |
| `QD_ERR_FOREVER` | an explode-forever or reroll-forever that every face passes |
| `QD_ERR_SUM` | the sum doesn't add up (low-level building gone wrong) |
| `QD_ERR_DIV_ZERO` | divide by the number 0 |
| `QD_ERR_PERCENT` | percentile dice with steps or advantage, or more than 3 bonus dice |
| `QD_ERR_REROLL` | that die can't be rerolled: not in play, not listed, or percentile |

```c example=err_text
for (int e = QD_OK; e <= QD_ERR_REROLL; e++)
    printf("%d: %s\n", e, qd_err_text((qd_err_t)e));
```

```text output
0: ok
1: too many pools, steps, buckets or terms
2: a pool needs at least 1 die with at least 1 side
3: too many dice
4: digit dice need 1-4 digits of 1-9 and can't explode or reroll
5: explodes or rerolls on every face, so it would never stop
6: the sum doesn't add up
7: divide by zero
8: percentile dice take bonus/penalty (up to 3) but no other modifiers
9: that die can't be rerolled
```

---

## qd_roll_warnings

Warnings: the roll is allowed, but probably not what was meant. [`qd_roll_run`](#qd_roll_run) also puts them in `res.warnings`.

```c
uint16_t qd_roll_warnings(const qd_roll_t *r);
const char *qd_warn_text(uint16_t w);
```

| Warning | Means |
|---|---|
| `QD_WARN_ONE_SIDED` | a d1, or a 1 in digit dice: it always shows 1 |
| `QD_WARN_KEEP_NONE` | keep 0: keeps nothing |

```c example=roll_warnings
static qd_roll_t roll;
qd_roll_init(&roll);
qd_roll_digits(&roll, 1, "61");             /* a d6 then a d1 */
printf("%s\n", qd_warn_text(qd_roll_warnings(&roll)));
```

```text output
a die with one side always shows the same number
```

`qd_warn_text` gives the text of the first warning set, or `""`.

---

## qd_reroll_all

Roll the whole thing again: a Savage Worlds benny, a Call of Cthulhu push. The result keeps its history.

```c
qd_err_t qd_reroll_all(const qd_roll_t *r, qd_result_t *res);
```

```c example=reroll_all
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_dice(&roll, 2, 6);
qd_roll_run(&roll, &res);
printf("rolled %ld\n", (long)res.total);
qd_reroll_all(&roll, &res);                 /* spend a benny */
printf("rerolled: %ld (was %ld, rerolls %u)\n", (long)res.total, (long)res.was_total, res.rerolls);
```

```text output
rolled 10
rerolled: 8 (was 10, rerolls 1)
```

`res` must hold the previous result of the same roll; it's replaced. Afterwards the result has `QD_RES_REROLLED`, `rerolls` counts the rerolls, and `was_total` / `was_successes` hold the result before the last one. For the game's reading, call [`qd_sys_outcome`](systems.md#qd_sys_outcome) again.

---

## qd_reroll_dice

Reroll chosen dice: Vampire's willpower, a "reroll any two dice" talent.

```c
qd_err_t qd_reroll_dice(const qd_roll_t *r, qd_result_t *res, const uint16_t *which, int n);
```

```c example=reroll_dice
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_success(qd_roll_dice(&roll, 5, 10), qd_test_ge(6));
qd_roll_run(&roll, &res);
printf("%d successes\n", res.successes);
uint16_t worst[2];                                  /* the two lowest dice */
for (int k = 0; k < 2; k++) {
    int best = -1;
    for (int i = 0; i < res.dice_count; i++)
        if ((k == 0 || i != worst[0]) && (best < 0 || res.dice[i].value < res.dice[best].value)) best = i;
    worst[k] = (uint16_t)best;
}
qd_reroll_dice(&roll, &res, worst, 2);
for (int i = 0; i < res.dice_count; i++)
    printf(res.dice[i].flags & QD_DIE_CHANGED ? "%d->%d " : "%d ",
           res.dice[i].flags & QD_DIE_CHANGED ? res.dice[i].was : res.dice[i].value, res.dice[i].value);
printf(": %d successes\n", res.successes);
```

```text output
4 successes
10 6->3 10 3->10 7 : 4 successes
```

| Parameter | Meaning |
|---|---|
| `which`, `n` | the dice, as indexes into `res->dice` |

**Returns** `QD_OK`, or `QD_ERR_REROLL` if any of them can't be rerolled (out of play other than dropped, not listed, or percentile): then nothing changes.

Each chosen die gets a fresh roll and keeps its first value in `was`, flagged `QD_DIE_CHANGED`; then the pool is worked out again (keep or drop, counting, sets, buckets, the sum). A rerolled die doesn't explode again, and any dice that came from it are set aside.

---

## qd_reroll_matching

Reroll every die in play that passes a test: Year Zero's push rerolls everything that isn't a 6.

```c
int qd_reroll_matching(const qd_roll_t *r, qd_result_t *res, int pool, qd_test_t t);
```

```c example=reroll_matching
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_success(qd_roll_dice(&roll, 6, 6), qd_test_eq(6));
qd_roll_run(&roll, &res);
printf("%d sixes\n", res.successes);
int n = qd_reroll_matching(&roll, &res, -1, qd_test_ne(6));    /* push */
printf("pushed %d dice: %d sixes\n", n, res.successes);
```

```text output
3 sixes
pushed 3 dice: 3 sixes
```

| Parameter | Meaning |
|---|---|
| `pool` | which pool, or -1 for all of them |
| `t` | which dice to reroll |

**Returns** how many dice were rerolled (0 or more), or -1 on an error.

---

## qd_reroll_keep

Roll again and keep the better (or worse) of the two: luck, the Lucky feat, fortune points.

```c
bool qd_reroll_keep(const qd_roll_t *r, qd_result_t *res, bool higher);
```

```c example=reroll_keep
static qd_roll_t roll;
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_roll_dice(&roll, 1, 20);
qd_roll_run(&roll, &res);
printf("first roll:  %ld\n", (long)res.total);
bool took = qd_reroll_keep(&roll, &res, true);       /* Lucky: roll again, keep the higher */
printf("second roll: %ld\n", (long)(took ? res.total : res.was_total));
printf("kept: %ld\n", (long)res.total);
```

```text output
first roll:  19
second roll: 11
kept: 19
```

| Parameter | Meaning |
|---|---|
| `higher` | `true`: keep the better. `false`: the worse |

**Returns** `true` if the new roll was kept. If the first was, `res` holds the first roll again with `QD_RES_KEPT_FIRST` set. Either way, `was_total` is the other roll's total. Counting rolls compare successes, others the total; a tie keeps the new roll.

---

## qd_roll_observe

Watch every die the engine rolls: kept or not, explosions and rerolls included. [Stats for nerds](stats.md) is built on this.

```c
typedef void (*qd_observer_t)(uint16_t sides, const qd_die_t *die, uint16_t face, int16_t value, void *ctx);
qd_observer_t qd_roll_observe(qd_observer_t fn, void *ctx, void **old_ctx);
```

```c example=roll_observe
static int ones;
static void count_ones(uint16_t sides, const qd_die_t *die, uint16_t face, int16_t value, void *ctx)
{
    if (value == 1) ones++;
}

int main(void)
{
    static qd_roll_t roll;
    static qd_result_t res;
    qd_rng_init_hw();
    qd_roll_observe(count_ones, NULL, NULL);
    qd_roll_init(&roll);
    qd_roll_dice(&roll, 60, 6);
    qd_roll_run(&roll, &res);
    printf("%d ones in 60 dice\n", ones);
    qd_roll_observe(NULL, NULL, NULL);
    return 0;
}
```

```text output
5 ones in 60 dice
```

| Parameter | Meaning |
|---|---|
| `fn` | called once per die rolled, or `NULL` for none |
| `ctx` | passed to `fn` |
| `old_ctx` | if not `NULL`, gets the previous observer's `ctx` |

**Returns** the previous observer, so you can chain to it or put it back. Percentile dice are reported as the two d10s they are (0 read as 10), digit dice as one die per digit. Rolls made while working out [odds](odds.md) are never reported.

---

## qd_pool_range

The lowest and highest value a pool's die can show.

```c
void qd_pool_range(const qd_pool_t *p, int16_t *lo, int16_t *hi);
```

```c example=pool_range
static qd_roll_t roll;
int16_t lo, hi;
qd_roll_init(&roll);
qd_roll_custom(&roll, 4, qd_fudge_die());
qd_pool_range(&roll.pools[0], &lo, &hi);
printf("a Fudge die: %d to %d\n", lo, hi);
```

```text output
a Fudge die: -1 to 1
```

---

## qd_roll_new_pool

Low level: add a pool without touching the sum, and write the sum yourself. This is how [the parser](parse.md) gets brackets and precedence right.

```c
qd_pool_t *qd_roll_new_pool(qd_roll_t *r);
bool qd_roll_push(qd_roll_t *r, qd_op_kind_t kind, int32_t arg);
```

The sum is a short program run on a stack: `QD_OP_POOL n` pushes pool n's total, `QD_OP_NUMBER n` pushes n, `QD_OP_ADD`, `_SUB`, `_MUL`, `_DIV` and `QD_OP_MAX` combine the top two, `QD_OP_NEG` negates the top. Up to 24 steps (`QD_MAX_ROLL_OPS`).

```c example=roll_push
static qd_roll_t roll;                      /* 1d20 + (1d4 + 1) * 2, with the brackets */
static qd_result_t res;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_t *a = qd_roll_new_pool(&roll); a->count = 1; a->sides = 20;
qd_pool_t *b = qd_roll_new_pool(&roll); b->count = 1; b->sides = 4;
qd_roll_push(&roll, QD_OP_POOL, 0);
qd_roll_push(&roll, QD_OP_POOL, 1);
qd_roll_push(&roll, QD_OP_NUMBER, 1);
qd_roll_push(&roll, QD_OP_ADD, 0);
qd_roll_push(&roll, QD_OP_NUMBER, 2);
qd_roll_push(&roll, QD_OP_MUL, 0);
qd_roll_push(&roll, QD_OP_ADD, 0);
qd_roll_run(&roll, &res);
printf("%d + (%d + 1) * 2 = %ld\n", res.dice[0].value, res.dice[1].value, (long)res.total);
```

```text output
19 + (3 + 1) * 2 = 27
```

**Returns** the pool (`NULL` when full), and `false` from `qd_roll_push` when the sum is full. A sum that doesn't leave exactly one number is refused with `QD_ERR_SUM`.
