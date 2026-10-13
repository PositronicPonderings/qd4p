# Notation: the parser

[parse](#qd_parse) · [parsed roll](#qd_parsed_t) · [errors](#qd_parse_error_t) · [several rolls](#qd_parse_line) · [repeats and sets](#qd_set_run) · [mass rolls](#qd_mass_run) · [write it back](#qd_format) · [one pool](#qd_format_pool) · [custom dice by name](#qd_parse_set_lookup)

Header: `qd_parse.h` (included by `qd4p.h`).

**The parser, in three lines:**
1. It turns text like `4d6k3 + 2` or `atk adv + 5 vs 15` into a **roll**, the same `qd_roll_t` the [roll engine](rolls.md) builds by hand, ready for [`qd_roll_run`](rolls.md#qd_roll_run).
2. When the text isn't a roll, it says **where** and **why**, in words a player understands: "no space inside dice: write 4d7".
3. **Parse once, run as often as you like.** Parsing is the slow part (still well under a millisecond); rolling is fast.

Everything the notation can say, with examples of each: [Part 5, Dice notation](../05-notation.md). This page is the C side.

The parser is the biggest part of QD4P, about 15 KB, and it brings in every game system. An app with fixed rolls doesn't need it: [what it costs](../../SIZES.md).

---

## qd_parse

Text in, a roll out.

```c
bool qd_parse(const char *text, qd_parsed_t *out, qd_parse_error_t *err);
```

```c example=parse
static qd_parsed_t p;
static qd_result_t res;
qd_parse_error_t err;
qd_rng_init_hw();
if (qd_parse("(Fireball) 8d6 ! everyone in the room", &p, &err)) {
    qd_roll_run(&p.roll, &res);
    printf("%s: %ld (%s)\n", p.label, (long)res.total, p.comment);
} else {
    printf("column %d: %s\n", err.pos + 1, err.message);
}
```

```text output
Fireball: 35 (everyone in the room)
```

| Parameter | Meaning |
|---|---|
| `text` | the notation. Spaces are optional and case doesn't matter |
| `out` | the roll and what was written around it |
| `err` | where and what the problem is, when there is one. May be `NULL` |

**Returns** `true` if `text` is one roll. A `;` (several rolls) is an error here: use [`qd_parse_line`](#qd_parse_line).

---

## qd_parsed_t

A parsed roll: the roll itself, and what was written around it.

```c
typedef struct {
    qd_roll_t roll;            /* ready for qd_roll_run */
    uint16_t  repeat;          /* "6 x 4d6k3": 6; otherwise 1. Over 20: a mass roll */
    char      label[24];       /* "(Bite) 1d20+5": "Bite" */
    char      comment[48];     /* "1d20 ! flanking": "flanking" */
    char      die_names[8][16];/* d{name} per pool, "" if none */
    bool      set;             /* "(6 x 4d6) k3": the repeats are a set */
    uint8_t   set_keep;        /* the set's keep or drop (a qd_keep_t) */
    uint16_t  set_keep_n;
} qd_parsed_t;
```

It's 1.6 KB: make it `static` or global. For a roll with a repeat or a set, run it with [`qd_set_run`](#qd_set_run) or [`qd_mass_run`](#qd_mass_run); `qd_roll_run` on `p.roll` rolls it once.

---

## qd_parse_error_t

Where the problem is, and what it is.

```c
typedef struct {
    uint16_t pos, len;         /* where: from 0, so show pos + 1 as a column; how many characters */
    char     message[64];      /* what, in words */
} qd_parse_error_t;
```

```c example=parse_error
static qd_parsed_t p;
qd_parse_error_t err;
const char *text = "4d6 k3 +";
if (!qd_parse(text, &p, &err)) {
    printf("%s\n%*s", text, err.pos, "");
    for (int i = 0; i < err.len; i++) putchar('^');
    printf(" %s\n", err.message);
}
```

```text output
4d6 k3 +
        ^ the roll stops too soon: a number, dice or '(' should come next
```

Underlining the problem is how the console shows it, and how a player learns the notation without reading anything. Every message is listed in [the notation guide](../05-notation.md#errors).

---

## qd_parse_line

Several rolls on one line, separated by `;`: `2d6 ; 3d8 ; 1d20`.

```c
int qd_parse_line(const char *text, qd_parsed_t *out, int max, qd_parse_error_t *err);
```

```c example=parse_line
static qd_parsed_t rolls[4];
static qd_result_t res;
qd_parse_error_t err;
qd_rng_init_hw();
int n = qd_parse_line("(Attack) 1d20+5 ; (Damage) 1d8+3", rolls, 4, &err);
for (int k = 0; k < n; k++) {
    qd_roll_run(&rolls[k].roll, &res);
    printf("%-7s %ld\n", rolls[k].label, (long)res.total);
}
```

```text output
Attack  24
Damage  8
```

| Parameter | Meaning |
|---|---|
| `out`, `max` | room for `max` parsed rolls |

**Returns** how many rolls, or -1 with `err` filled in (its position is in the whole line). More than `max` rolls is an error.

---

## qd_set_run

Roll a parsed roll with its repeat or set: `6 x 4d6k3` rolls six times; `(7 x 4d6k3) k6` rolls seven and keeps the best six.

```c
qd_err_t qd_set_run(const qd_parsed_t *p, qd_set_result_t *out, qd_set_each_t each, void *ctx);

typedef struct {
    uint8_t  count;            /* rolls made */
    bool     counting;         /* the values are successes */
    int32_t  total;            /* the kept values added up */
    int32_t  value[20];        /* each roll's total (or successes) */
    bool     kept[20];         /* which the set kept */
} qd_set_result_t;

typedef void (*qd_set_each_t)(int index, const qd_result_t *res, void *ctx);
```

```c example=set_run
static qd_parsed_t p;
qd_set_result_t s;
qd_parse_error_t err;
qd_rng_init_hw();
qd_parse("(7 x 4d6k3) k6", &p, &err);              /* seven stats, drop the worst */
qd_set_run(&p, &s, NULL, NULL);
for (int i = 0; i < s.count; i++) printf(s.kept[i] ? "%ld " : "(%ld) ", (long)s.value[i]);
printf("= %ld\n", (long)s.total);
```

```text output
16 15 12 13 16 (11) 15 = 87
```

| Parameter | Meaning |
|---|---|
| `each` | if not `NULL`, called with every roll's full result as it's rolled, to show the dice; `index` counts from 0 |
| `ctx` | passed to `each` |

**Returns** `QD_OK`, or the [error](rolls.md#qd_err_t) that stopped it. A plain roll (no repeat) is a set of one. Ties in a set's keep or drop follow the dice rule: "lowest" is the earliest, "highest" the latest. Up to 20 rolls (`QD_MAX_REPEAT`); more is a [mass roll](#qd_mass_run).

---

## qd_mass_run

Many repeats, counted rather than listed: `400 x atk vs 12`, a horde's attacks.

```c
qd_err_t qd_mass_run(const qd_parsed_t *p, qd_mass_result_t *out);

typedef struct {
    uint16_t count;            /* rolls made */
    bool     counting;         /* the values are successes */
    int64_t  sum;              /* of the values */
    int32_t  lo, hi;           /* lowest and highest value */
    bool     judged;           /* a target or system: passes and levels count */
    uint16_t passes;
    uint16_t level[9];         /* rolls with outcome level L at [L + 4] */
} qd_mass_result_t;
```

```c example=mass_run
static qd_parsed_t p;
static qd_mass_result_t m;
qd_parse_error_t err;
qd_rng_init_hw();
qd_parse("400 x atk + 4 vs 15", &p, &err);
qd_mass_run(&p, &m);
printf("%u attacks: %u hit, %u of them critical\n", m.count, m.passes, m.level[QD_LVL_CRIT + 4]);
printf("average roll %.2f, from %ld to %ld\n", (double)m.sum / m.count, (long)m.lo, (long)m.hi);
```

```text output
400 attacks: 204 hit, 19 of them critical
average roll 14.60, from 5 to 24
```

Up to 1,000 rolls (`QD_MAX_MASS`). Each is a full roll by the same rules as one; only the counting is different. [Outcome levels](systems.md#outcome-levels) say what `level[]` holds.

---

## qd_format

Write a parsed roll back as tidy notation: `4d6K3+2` comes back as `4d6 k3 + 2`. Parsing what it writes gives the same roll.

```c
int qd_format(const qd_parsed_t *p, char *buf, int size);
```

```c example=format
static qd_parsed_t p;
qd_parse_error_t err;
char tidy[80];
const char *typed[] = { "4D6K3+2", "(bite)1d20+14!vs AC 15", "6x4d6k3", "sw8+1x v6", "coc65b2p1" };
for (int i = 0; i < 5; i++) {
    qd_parse(typed[i], &p, &err);
    qd_format(&p, tidy, sizeof tidy);
    printf("%-24s -> %s\n", typed[i], tidy);
}
```

```text output
4D6K3+2                  -> 4d6 k3 + 2
(bite)1d20+14!vs AC 15   -> (bite) 1d20 + 14 ! vs AC 15
6x4d6k3                  -> 6 x 4d6 k3
sw8+1x v6                -> sw8 x + 1 vs 6
coc65b2p1                -> coc 65 b1
```

**Returns** the length the whole roll needs: if that's `size` or more, `buf` holds as much as fits. Use it to show what the device understood, and to save rolls (a favourite, a macro) as text.

---

## qd_format_pool

One pool on its own, as notation: `4d6 k3`, `hunger:2d10`.

```c
int qd_format_pool(const qd_parsed_t *p, int pool, char *buf, int size);
```

```c example=format_pool
static qd_parsed_t p;
static qd_result_t res;
qd_parse_error_t err;
char name[32];
qd_rng_init_hw();
qd_parse("1d20 + 1d4 + 2d6 ie", &p, &err);
qd_roll_run(&p.roll, &res);
for (int k = 0; k < res.pool_count; k++) {
    qd_format_pool(&p, k, name, sizeof name);
    printf("%-8s %ld\n", name, (long)res.pools[k].total);
}
```

```text output
1d20     19
1d4      3
2d6 ie   12
```

---

## qd_parse_set_lookup

Custom dice by name: `2d{Loaded}` asks your function for the die called "Loaded".

```c
typedef const qd_die_t *(*qd_die_lookup_t)(const char *name, void *ctx);
void qd_parse_set_lookup(qd_die_lookup_t fn, void *ctx);
```

```c example=parse_set_lookup
static qd_die_t skull;
static const qd_die_t *my_dice(const char *name, void *ctx)
{
    return strcmp(name, "skull") == 0 ? &skull : NULL;
}

int main(void)
{
    static const qd_face_t faces[] = { { 0, 4, "-" }, { 1, 2, "X" } };      /* 1 in 3 is a skull */
    static qd_parsed_t p;
    static qd_result_t res;
    qd_parse_error_t err;
    qd_rng_init_hw();
    qd_die_custom(&skull, faces, 2);
    qd_parse_set_lookup(my_dice, NULL);
    if (qd_parse("6d{skull} t1", &p, &err)) {
        qd_roll_run(&p.roll, &res);
        printf("%d skulls\n", res.successes);
    }
    printf("%s\n", qd_parse("2d{ghost}", &p, &err) ? "?" : err.message);
    return 0;
}
```

```text output
3 skulls
no custom die called 'ghost'
```

| Parameter | Meaning |
|---|---|
| `fn` | returns the die called `name` (the text between the braces, spaces trimmed), or `NULL` for "no such die". `NULL` turns custom dice off |
| `ctx` | passed to `fn` |

Names match however your function likes: exact, any case, by number. The die it returns must stay alive while the roll is used.
