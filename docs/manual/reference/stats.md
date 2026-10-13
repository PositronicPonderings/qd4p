# Stats for nerds

[the tallies](#qd_stats_t) · [clear](#qd_stats_clear) · [watch](#qd_stats_watch) · [add by hand](#qd_stats_add) · [find a die](#qd_stats_find) · [report](#qd_stats_report) · [the limit](#qd_chi_limit)

Header: `qd_stats.h` (included by `qd4p.h`).

**Stats for nerds, in three lines:**
1. Every die the engine rolls is **tallied** by kind (d6, d20, your loaded die): how many times, the total, each face.
2. A **report** compares a kind with what a fair die (or a fairly weighted one) would do: the average, the spread, natural highs and lows, and a fairness check.
3. It answers the question every table asks sooner or later: "is this thing cursed?" Usually, no. Now you can show it.

A guide for players, with what the numbers mean: [Part 6, Odds and stats for nerds](../06-odds-and-stats.md#stats-for-nerds).

---

## qd_stats_t

The tallies: one entry per kind of die.

```c
typedef struct {
    uint16_t       sides;            /* faces */
    const qd_die_t *die;             /* the custom die, or NULL for a standard one */
    uint32_t       count;            /* rolls */
    int64_t        sum;
    uint64_t       sum_sq;           /* the sum of each value squared */
    int16_t        min, max;         /* lowest and highest seen */
    bool           faces;            /* face[] is kept (up to 100 faces, QD_STATS_FACES) */
    uint32_t       face[100];        /* rolls of each face */
} qd_die_stats_t;

typedef struct {
    uint8_t        kinds;            /* entries used */
    uint32_t       untracked;        /* rolls of kinds past the 16th (QD_STATS_KINDS) */
    qd_die_stats_t kind[16];
} qd_stats_t;
```

A `qd_stats_t` is 7 KB: make it `static` or global, and [shrink it](../../SIZES.md#making-it-smaller) if you need the room.

**What counts:** every die the engine rolls, kept or not, exploded or rerolled, because every one is a real roll of that die. Percentile dice count as two d10s, digit dice (`dd66`) as one die per digit. Dice are told apart by their sides, or by which custom die they are: a loaded d6 is not a d6.

---

## qd_stats_clear

Start again from nothing.

```c
void qd_stats_clear(qd_stats_t *s);
```

---

## qd_stats_watch

Count every die the engine rolls, from now on.

```c
void qd_stats_watch(qd_stats_t *s);
```

```c example=stats_watch
static qd_stats_t stats;
static qd_parsed_t p;
static qd_result_t res;
qd_parse_error_t err;
qd_rng_init_hw();
qd_stats_clear(&stats);
qd_stats_watch(&stats);
qd_parse("1d20 + 2d6", &p, &err);
for (int i = 0; i < 300; i++) qd_roll_run(&p.roll, &res);
for (int k = 0; k < stats.kinds; k++)
    printf("d%u: %lu rolls, average %.2f\n", stats.kind[k].sides, (unsigned long)stats.kind[k].count,
           (double)stats.kind[k].sum / stats.kind[k].count);
```

```text output
d20: 300 rolls, average 10.38
d6: 600 rolls, average 3.55
```

| Parameter | Meaning |
|---|---|
| `s` | where to count, or `NULL` to stop counting |

It uses the engine's [observer](rolls.md#qd_roll_observe), so there's one watcher at a time. Rolls made while working out [odds](odds.md) are never counted.

---

## qd_stats_add

Count one die by hand: what watching does for each die. For dice rolled outside the engine ([`qd_die_roll`](dice.md#qd_die_roll), [`qd_rng_between`](randomness.md#qd_rng_between)).

```c
void qd_stats_add(qd_stats_t *s, uint16_t sides, const qd_die_t *die, uint16_t face, int16_t value);
```

```c example=stats_add
static qd_stats_t stats;
qd_rng_init_hw();
qd_stats_clear(&stats);
for (int i = 0; i < 600; i++) {
    int v = (int)qd_rng_between(1, 6);
    qd_stats_add(&stats, 6, NULL, (uint16_t)(v - 1), (int16_t)v);
}
printf("sixes: %lu of %lu\n", (unsigned long)stats.kind[0].face[5], (unsigned long)stats.kind[0].count);
```

```text output
sixes: 101 of 600
```

| Parameter | Meaning |
|---|---|
| `sides` | the die's faces |
| `die` | the custom die, or `NULL` for a standard one |
| `face`, `value` | which face came up (0 is the first) and its value |

---

## qd_stats_find

The tallies for one kind of die.

```c
const qd_die_stats_t *qd_stats_find(const qd_stats_t *s, uint16_t sides, const qd_die_t *die);
```

**Returns** the kind for a standard die of `sides` (with `die` `NULL`) or for a custom die, or `NULL` if it hasn't been rolled. Example: [`qd_stats_report`](#qd_stats_report).

---

## qd_stats_report

How one kind of die is behaving.

```c
void qd_stats_report(const qd_die_stats_t *k, qd_stats_report_t *r);

typedef struct {
    double   mean, expect_mean;      /* the average roll, and what a fair die averages */
    double   sd, expect_sd;          /* the spread, and a fair die's */
    double   chi, limit;             /* the chi-square, and the 1-in-1,000 limit */
    int      df;                     /* degrees of freedom: faces - 1 */
    uint32_t highest, lowest;        /* rolls of the top and bottom face: natural 20s and 1s */
    uint8_t  verdict;                /* QD_STATS_* */
} qd_stats_report_t;
```

```c example=stats_report
static qd_stats_t stats;
static qd_roll_t roll;
static qd_result_t res;
qd_stats_report_t rep;
qd_rng_init_hw();
qd_stats_clear(&stats);
qd_stats_watch(&stats);
qd_roll_init(&roll);
qd_roll_dice(&roll, 1, 20);
for (int i = 0; i < 2000; i++) qd_roll_run(&roll, &res);
qd_stats_report(qd_stats_find(&stats, 20, NULL), &rep);
printf("average %.2f (fair: %.2f), natural 20s: %lu\n", rep.mean, rep.expect_mean, (unsigned long)rep.highest);
printf("chi-square %.1f, limit %.1f: %s\n", rep.chi, rep.limit,
       rep.verdict == QD_STATS_FAIR ? "fair" : rep.verdict == QD_STATS_ODD ? "odd" : "can't say yet");
```

```text output
average 10.74 (fair: 10.50), natural 20s: 112
chi-square 26.3, limit 43.8: fair
```

| Verdict | Means |
|---|---|
| `QD_STATS_TOO_FEW` | not enough rolls to judge yet: it needs about 5 per face (30 for a d6, 100 for a d20) |
| `QD_STATS_FAIR` | behaving |
| `QD_STATS_ODD` | counts this uneven happen to a fair die less than 1 time in 1,000 |
| `QD_STATS_NO_FACES` | too many faces to check one by one (over 100) |

**Reading "odd".** A fair die is odd about 1 time in 1,000, so one odd is a curiosity, not a curse. The same die odd again and again, after clearing, is worth a look. For a weighted custom die, "fair" means "weighted the way it says".

---

## qd_chi_limit

The chi-square a fair die exceeds 1 time in 1,000, for `df` degrees of freedom: the line between "fair" and "odd".

```c
double qd_chi_limit(int df);
```

```c example=chi_limit
int sides[] = { 4, 6, 8, 10, 12, 20, 100 };
for (int i = 0; i < 7; i++) printf("d%-3d %.1f\n", sides[i], qd_chi_limit(sides[i] - 1));
```

```text output
d4   16.3
d6   20.5
d8   24.3
d10  27.9
d12  31.3
d20  43.8
d100 148.3
```

Table values up to 30 degrees of freedom; beyond, the Wilson-Hilferty approximation, within 0.3%. [How it works](../10-how-it-works.md#the-fairness-check) explains the test.
