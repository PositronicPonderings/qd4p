# Game systems

[the outcome](#qd_outcome_t) · [outcome levels](#outcome-levels) · [judge](#qd_sys_outcome) · [vs](#qd_roll_vs) · [D&D 5e](#qd_sys_dnd) · [Savage Worlds](#qd_sys_sw) · [Call of Cthulhu 7e](#qd_sys_coc) · [FATE](#qd_sys_fate) · [Vampire 5e](#qd_sys_vtm) · [DCC](#qd_sys_dcc) · [dice chain](#qd_dcc_chain) · [level names](#qd_sys_level_name) · [system names](#qd_sys_name) · [FATE ladder](#qd_fate_ladder)

Header: `qd_sys.h` (included by `qd4p.h`).

**Game systems, in three lines:**
1. A **builder** sets up a roll the way a game wants it: `qd_sys_sw(&roll, 8, true)` is a d8 trait die and a d6 wild die, both exploding, the better one counting.
2. [`qd_sys_outcome`](#qd_sys_outcome) reads a result the way that game does: "Success, 1 raise", "Hard success", "Messy critical", "Critical hit".
3. The parser builds exactly the same rolls from words (`sw8`, `coc 65 b1`, `atk adv`): [Game systems in notation](../05-notation.md#game-systems).

Each builder starts a fresh roll ([`qd_roll_init`](rolls.md#qd_roll_init) is called for you). Add maths and a target afterwards: `qd_roll_add(&roll, 5)`, [`qd_roll_vs(&roll, 15)`](#qd_roll_vs). Only the builders you call are linked into your program; judging links them all, about 4 KB.

---

## qd_outcome_t

What a result means.

```c
typedef struct {
    uint8_t  system;           /* QD_SYS_* */
    int8_t   level;            /* how good: see Outcome levels */
    bool     judged;           /* a target was known, so pass means something */
    bool     pass;
    bool     critical, fumble; /* natural 20, snake eyes, 01, a pair of 10s ... */
    int32_t  value;            /* what was judged: the total, or successes */
    int32_t  target;
    int32_t  margin;           /* value - target, when judged */
    int16_t  degrees;          /* Savage Worlds raises, FATE shifts, Vampire's margin */
    char     text[40];         /* the headline: "Extreme success" ("" if nothing to say) */
    char     detail[64];       /* more: "natural 20", "Good (+3), 2 shifts", "1 ace, vs 4" */
} qd_outcome_t;
```

`text` and `detail` are ready to show. The numbers are there for an app that wants to do more: colour the screen by `level`, play a sound on `critical`.

---

## Outcome levels

Every system's outcome has a **level**: higher is better, 0 means "nothing to judge", below 0 is a failure. So an app can treat every game the same way ("level > 0: green") and still name each result properly with [`qd_sys_level_name`](#qd_sys_level_name).

| Level | Plain `vs N` | D&D attack | D&D check, save | Savage Worlds | DCC | Call of Cthulhu | FATE (with `vs`) | Vampire 5e |
|---:|---|---|---|---|---|---|---|---|
| 4 | | | | | | Critical success | | |
| 3 | | Critical hit | | | Critical | Extreme success | Success with style | Critical |
| 2 | | | | Raise | | Hard success | Success | Messy critical |
| 1 | Pass | Hit | Success | Success | Success | Regular success | Tie | Win |
| 0 | | | | | | | | |
| -1 | Fail | Miss | Failure | Failure | Failure | Failure | Fail | Failure |
| -2 | | | | Critical failure | Fumble | Fumble | | Total failure |
| -3 | | | | | | | | Bestial failure |

The names in C: `QD_LVL_CRIT_FAIL` (-2), `QD_LVL_FAIL`, `QD_LVL_NONE`, `QD_LVL_PASS`, `QD_LVL_RAISE`, `QD_LVL_CRIT` (3) for the first five columns; `QD_COC_FUMBLE` to `QD_COC_CRITICAL`; `QD_FATE_FAIL` to `QD_FATE_STYLE`; `QD_VTM_BESTIAL` to `QD_VTM_CRITICAL`. [Odds](odds.md#qd_odds) and [mass rolls](parse.md#qd_mass_run) give the chance or count of each level at `[level + 4]`.

---

## qd_sys_outcome

Read a result the way its game does.

```c
bool qd_sys_outcome(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out);
```

```c example=sys_outcome
static qd_roll_t roll;
static qd_result_t res;
qd_outcome_t out;
qd_rng_init_hw();
qd_sys_sw(&roll, 8, true);                 /* Fighting d8, a Wild Card */
qd_roll_add(&roll, 1);
qd_roll_vs(&roll, 6);                      /* against Parry 6 */
for (int i = 0; i < 4; i++) {
    qd_roll_run(&roll, &res);
    qd_sys_outcome(&roll, &res, &out);
    printf("%2ld  %-18s %s\n", (long)res.total, out.text, out.detail);
}
```

```text output
14  Success, 2 raises  2 aces, vs 6
 9  Success            1 ace, vs 6
 8  Success            vs 6
 5  Failure            vs 6
```

**Returns** `false` (with level 0 and empty texts) when there is nothing to judge: a plain roll with no `vs`. Every game system has something to say, so a system roll always returns `true`. After a [reroll](rolls.md#qd_reroll_all), call it again.

---

## qd_roll_vs

Give a roll a target: "vs 15". A plain roll passes if its total (or its successes, if it counts them) is the target or more; a game system reads it its own way.

```c
void qd_roll_vs(qd_roll_t *r, int32_t target);
```

```c example=roll_vs
static qd_roll_t roll;
static qd_result_t res;
qd_outcome_t out;
qd_rng_init_hw();
qd_roll_init(&roll);
qd_pool_success(qd_roll_dice(&roll, 6, 6), qd_test_ge(5));   /* 6d6, 5+ succeeds */
qd_roll_vs(&roll, 3);                                        /* need 3 successes */
qd_roll_run(&roll, &res);
qd_sys_outcome(&roll, &res, &out);
printf("%d successes: %s (%s)\n", res.successes, out.text, out.detail);
```

```text output
3 successes: Pass (exactly)
```

For Call of Cthulhu the target is the difficulty: `QD_COC_REGULAR`, `QD_COC_HARD` or `QD_COC_EXTREME`. For Savage Worlds it replaces the Target Number of 4.

---

## qd_sys_dnd

**Dungeons & Dragons 5th edition:** a d20, with advantage or disadvantage, for an attack, an ability check or a saving throw.

```c
bool qd_sys_dnd(qd_roll_t *r, int kind, int adv);
```

```c example=sys_dnd
static qd_roll_t roll;
static qd_result_t res;
qd_outcome_t out;
qd_rng_init_hw();
qd_sys_dnd(&roll, QD_DND_ATTACK, 1);       /* attack with advantage */
qd_roll_add(&roll, 5);                     /* +5 to hit */
qd_roll_vs(&roll, 15);                     /* AC 15 */
for (int i = 0; i < 5; i++) {
    qd_roll_run(&roll, &res);
    qd_sys_outcome(&roll, &res, &out);
    printf("d20 %2d, %2d: %2ld  %s %s\n", res.dice[0].value, res.dice[1].value, (long)res.total,
           out.text, out.detail);
}
```

```text output
d20 19, 11: 24  Hit
d20 20,  5: 25  Critical hit natural 20
d20 13, 20: 25  Critical hit natural 20
d20  5, 16: 21  Hit
d20  7,  7: 12  Miss
```

| Parameter | Meaning |
|---|---|
| `kind` | `QD_DND_ATTACK`, `QD_DND_CHECK` or `QD_DND_SAVE` |
| `adv` | 1 for advantage, -1 for disadvantage, 0 for neither (more than 1 is allowed, for house rules) |

**The rules it reads:** on an **attack**, a natural 20 is a critical hit and hits whatever the total; a natural 1 misses whatever the total. On a check or save there's no such rule, so a natural 20 or 1 is only noted in `detail`. "Natural" means the d20 that counted, after advantage set the other one aside.

**Returns** `false` if the roll has no room (never, from a fresh roll).

---

## qd_sys_sw

**Savage Worlds (Adventure Edition):** a trait die and, for a Wild Card, a d6 wild die. Both explode ("ace"); the better one counts.

```c
bool qd_sys_sw(qd_roll_t *r, int trait_sides, bool wild_card);
```

```c example=sys_sw
static qd_roll_t roll;
static qd_result_t res;
qd_outcome_t out;
qd_rng_init_hw();
qd_sys_sw(&roll, 6, true);                 /* Shooting d6 */
for (int i = 0; i < 5; i++) {
    qd_roll_run(&roll, &res);
    qd_sys_outcome(&roll, &res, &out);
    printf("trait %2ld, wild %2ld: %-18s %s\n", (long)res.pools[0].total, (long)res.pools[1].total,
           out.text, out.detail);
}
```

```text output
trait 10, wild  8: Success, 1 raise   2 aces, vs 4
trait  4, wild  8: Success, 1 raise   1 ace, vs 4
trait  5, wild  2: Success            vs 4
trait  2, wild  4: Success            vs 4
trait 11, wild  2: Success, 1 raise   1 ace, vs 4
```

| Parameter | Meaning |
|---|---|
| `trait_sides` | 4, 6, 8, 10 or 12. For d12+1 and up, add the bonus with `qd_roll_add` |
| `wild_card` | `true`: roll the wild die. `false`: an Extra, the trait die alone |

**The rules it reads:** Target Number 4, or the one from [`qd_roll_vs`](#qd_roll_vs). Every 4 over is a **raise**. A Wild Card rolling 1 on both dice is a **critical failure**, whatever the total ("snake eyes"). Each die that exploded is an **ace**, counted in `detail`. Modifiers apply to the better die, as the rules say (the total is the better die plus the modifiers).

**Returns** `false` for a trait die that isn't 4, 6, 8, 10 or 12.

---

## qd_sys_coc

**Call of Cthulhu 7th edition:** d% against a skill, with bonus and penalty dice and a difficulty.

```c
bool qd_sys_coc(qd_roll_t *r, int skill, int bonus);
```

```c example=sys_coc
static qd_roll_t roll;
static qd_result_t res;
qd_outcome_t out;
qd_rng_init_hw();
qd_sys_coc(&roll, 60, 1);                  /* Library Use 60, one bonus die */
qd_roll_vs(&roll, QD_COC_HARD);            /* a Hard difficulty */
for (int i = 0; i < 4; i++) {
    qd_roll_run(&roll, &res);
    qd_sys_outcome(&roll, &res, &out);
    printf("%3ld  %-36s %s\n", (long)res.total, out.text, out.detail);
}
```

```text output
 59  Failure (Regular success, Hard needed) skill 60: hard 30, extreme 12
 62  Failure                              skill 60: hard 30, extreme 12
 32  Failure (Regular success, Hard needed) skill 60: hard 30, extreme 12
 63  Failure                              skill 60: hard 30, extreme 12
```

| Parameter | Meaning |
|---|---|
| `skill` | the skill or characteristic: 1 to 99 (up to 999 for odd cases) |
| `bonus` | bonus dice (positive) or penalty dice (negative), up to 3 |

**The rules it reads:** at or under the skill is a Regular success, under half a Hard, under a fifth an Extreme, a 01 a Critical. A Fumble is 100, or 96 to 100 when the skill is under 50. With a difficulty (`vs`), the result must reach it: a Regular success against a Hard difficulty is a failure, and `text` says so. A Critical always succeeds and a Fumble always fails. `margin` is how far under the skill the roll was.

---

## qd_sys_fate

**FATE:** four Fudge dice. Add the skill; against opposition, the difference is **shifts**.

```c
bool qd_sys_fate(qd_roll_t *r);
```

```c example=sys_fate
static qd_roll_t roll;
static qd_result_t res;
qd_outcome_t out;
qd_rng_init_hw();
qd_sys_fate(&roll);
qd_roll_add(&roll, 2);                     /* Athletics: Fair (+2) */
qd_roll_run(&roll, &res);
qd_sys_outcome(&roll, &res, &out);
printf("no opposition: %s\n", out.text);
qd_roll_vs(&roll, 3);                      /* against Good (+3) */
for (int i = 0; i < 3; i++) {
    qd_roll_run(&roll, &res);
    qd_sys_outcome(&roll, &res, &out);
    printf("vs Good: %-20s %s\n", out.text, out.detail);
}
```

```text output
no opposition: Good (+3)
vs Good: Tie                  Good (+3), 0 shifts
vs Good: Fail                 Average (+1), -2 shifts
vs Good: Fail                 Fair (+2), -1 shift
```

**The rules it reads:** the total is read on the ladder (Good +3, Great +4 ...). With opposition, fewer shifts than 0 fail, 0 ties, 1 or 2 succeed, 3 or more succeed with style. `degrees` holds the shifts.

---

## qd_sys_vtm

**Vampire: the Masquerade 5th edition:** a pool of d10s, some of them hunger dice.

```c
bool qd_sys_vtm(qd_roll_t *r, int pool, int hunger);
```

```c example=sys_vtm
static qd_roll_t roll;
static qd_result_t res;
qd_outcome_t out;
qd_rng_init_hw();
qd_sys_vtm(&roll, 7, 2);                   /* 7 dice, 2 of them hunger */
qd_roll_vs(&roll, 3);                      /* difficulty 3 */
for (int i = 0; i < 4; i++) {
    qd_roll_run(&roll, &res);
    qd_sys_outcome(&roll, &res, &out);
    printf("%-16s %s\n", out.text, out.detail);
}
```

```text output
Messy critical   7 successes, 4 from a pair of 10s, vs 3
Win              4 successes vs 3
Win              5 successes vs 3
Win              3 successes vs 3
```

| Parameter | Meaning |
|---|---|
| `pool` | dice in the pool |
| `hunger` | how many of them are hunger dice (never more than the pool) |

**The rules it reads:** each 6 or more is a success; each **pair of 10s** is a critical, worth 4 successes instead of 2. A critical win with a 10 on a hunger die is a **messy critical**. A failure with a 1 on a hunger die is a **bestial failure**. No successes at all is a total failure. The hunger dice are their own pool, named `hunger`, so an app can colour them.

---

## qd_sys_dcc

**Dungeon Crawl Classics:** dice moved along the dice chain.

```c
bool qd_sys_dcc(qd_roll_t *r, int count, int sides, int steps);
```

```c example=sys_dcc
static qd_roll_t roll;
static qd_result_t res;
qd_outcome_t out;
qd_rng_init_hw();
qd_sys_dcc(&roll, 1, 20, 1);               /* a d20, one step up the chain: a d24 */
qd_roll_vs(&roll, 15);
for (int i = 0; i < 3; i++) {
    qd_roll_run(&roll, &res);
    qd_sys_outcome(&roll, &res, &out);
    printf("%2ld  %-9s %s\n", (long)res.total, out.text, out.detail);
}
```

```text output
23  Success   natural 23 on d24 (d20 +1d)
13  Failure   natural 13 on d24 (d20 +1d)
23  Success   natural 23 on d24 (d20 +1d)
```

| Parameter | Meaning |
|---|---|
| `count`, `sides` | the dice before moving: `1, 20` for a d20 |
| `steps` | how far along the chain: +1 is the next die up, -1 the next down |

**The rules it reads:** a natural roll of the die's highest face is a **critical**, a natural 1 a **fumble**; the judge rules on what that means. **Returns** `false` if `sides` isn't on the chain.

---

## qd_dcc_chain

The DCC dice chain: d3, d4, d5, d6, d7, d8, d10, d12, d14, d16, d20, d24, d30.

```c
int qd_dcc_chain(int sides, int steps);
```

```c example=dcc_chain
for (int s = -2; s <= 3; s++)
    printf("d20 %+d: d%d\n", s, qd_dcc_chain(20, s));
```

```text output
d20 -2: d14
d20 -1: d16
d20 +0: d20
d20 +1: d24
d20 +2: d30
d20 +3: d30
```

**Returns** the die `steps` along from `sides`, stopping at either end (a d30 can't go higher), or 0 if `sides` isn't on the chain.

---

## qd_sys_level_name

A short name for an outcome level of this roll's system: "Critical hit", "Raise", "Hard success", "Messy critical". For showing [odds](odds.md) and [mass rolls](parse.md#qd_mass_run).

```c
const char *qd_sys_level_name(const qd_roll_t *r, int level);
```

```c example=sys_level_name
static qd_roll_t roll;
qd_sys_coc(&roll, 50, 0);
for (int level = 4; level >= -2; level--)
    if (*qd_sys_level_name(&roll, level)) printf("%+d %s\n", level, qd_sys_level_name(&roll, level));
```

```text output
+4 Critical success
+3 Extreme success
+2 Hard success
+1 Regular success
-1 Failure
-2 Fumble
```

**Returns** the name, or `""` for a level the system doesn't use.

---

## qd_sys_name

The name of a system: "D&D 5e", "Savage Worlds", "Call of Cthulhu 7e", "FATE", "Vampire 5e", "DCC".

```c
const char *qd_sys_name(int id);
```

```c example=sys_name
for (int id = QD_SYS_DND; id <= QD_SYS_DCC; id++) printf("%d %s\n", id, qd_sys_name(id));
```

```text output
1 D&D 5e
2 Savage Worlds
3 Call of Cthulhu 7e
4 FATE
5 Vampire 5e
6 DCC
```

**Returns** the name, or `""` for `QD_SYS_NONE` and anything unknown. A roll's system is `roll.sys.id`.

---

## qd_fate_ladder

The FATE ladder: 3 is "Good".

```c
const char *qd_fate_ladder(int value);
```

```c example=fate_ladder
for (int v = -2; v <= 8; v++) printf("%+d %s\n", v, qd_fate_ladder(v));
```

```text output
-2 Terrible
-1 Poor
+0 Mediocre
+1 Average
+2 Fair
+3 Good
+4 Great
+5 Superb
+6 Fantastic
+7 Epic
+8 Legendary
```

From -2 (Terrible) to +8 (Legendary); beyond either end, the end's name.
