# Single dice

[types](#qd_die_t) · [standard](#qd_die_standard) · [custom](#qd_die_custom) · [Fudge](#qd_die_fudge) · [shared Fudge die](#qd_fudge_die) · [roll](#qd_die_roll) · [face value](#qd_die_face_value) · [face label](#qd_die_face_label) · [face chance](#qd_die_face_ppm) · [percentile](#qd_roll_percentile)

Header: `qd_dice.h` (included by `qd4p.h`).

**Single dice, in three lines:**
1. A **die** is a `qd_die_t`: a standard one (faces 1 to N), or a **custom** one with up to 30 faces of your choosing, each with a value, a weight and a label.
2. Rolling one gives the **face** that came up (0 is the first) and its **value**. Two faces can share a value; the face number keeps them apart, which matters for labels and for fairness checks.
3. For anything more than one die at a time (4d6 keep 3, exploding, successes), use [the roll engine](rolls.md): it uses these dice underneath.

---

## qd_die_t

The types: one face of a custom die, a die, and the result of rolling it.

```c
typedef struct {
    int16_t value;            /* what the face counts as */
    uint8_t weight;           /* its relative chance: 3 is three times as likely as 1; 0 counts as 1 */
    char    label[5];         /* shown instead of the value, if not "": "+", "Skull", "Hit" */
} qd_face_t;

typedef struct {
    uint16_t  id;             /* yours: an id for the die (0 = none) */
    uint16_t  sides;          /* how many faces */
    bool      custom;         /* false: faces are 1 .. sides */
    bool      weighted;       /* true if any weight isn't 1 */
    uint16_t  total_weight;   /* the weights added up (custom dice) */
    qd_face_t faces[30];      /* custom dice only (QD_MAX_FACES) */
} qd_die_t;

typedef struct {
    uint16_t face;            /* which face came up, 0 .. sides - 1 */
    int16_t  value;           /* its value */
} qd_face_roll_t;
```

A `qd_die_t` is 248 bytes: declare one per custom die you keep. Labels are up to 4 characters (`QD_MAX_LABEL`).

---

## qd_die_standard

A standard die: faces 1 to `sides`, all equally likely.

```c
bool qd_die_standard(qd_die_t *die, uint16_t sides);
```

```c example=die_standard
qd_die_t d30;
qd_rng_init_hw();
qd_die_standard(&d30, 30);          /* Dungeon Crawl Classics has these */
qd_face_roll_t r = qd_die_roll(&d30);
printf("d30: %d (face %u)\n", r.value, r.face);
```

```text output
d30: 29 (face 28)
```

| Parameter | Meaning |
|---|---|
| `die` | the die to fill in |
| `sides` | 1 to 32,767. Any size: d3, d7, d30 and d1000 work the same way as d6 |

**Returns** `false` (and leaves `die` alone) for 0 sides or more than 32,767.

---

## qd_die_custom

A die of your own: up to 30 faces, each with a value, a weight and a label. The faces are copied in.

```c
bool qd_die_custom(qd_die_t *die, const qd_face_t *faces, uint16_t count);
```

```c example=die_custom
static const qd_face_t loaded[] = {
    { 1, 1, "" }, { 2, 1, "" }, { 3, 1, "" }, { 4, 1, "" }, { 5, 1, "" }, { 6, 3, "" } };
static const qd_face_t combat[] = {             /* a 6-sided hit die: 3 blanks, 2 hits, 1 crit */
    { 0, 3, "-" }, { 1, 2, "Hit" }, { 2, 1, "Crit" } };
qd_die_t die_loaded, die_combat;
qd_rng_init_hw();
qd_die_custom(&die_loaded, loaded, 6);
qd_die_custom(&die_combat, combat, 3);
for (int i = 0; i < 5; i++) {
    qd_face_roll_t a = qd_die_roll(&die_loaded), b = qd_die_roll(&die_combat);
    printf("loaded %d   combat %s (%d)\n", a.value, qd_die_face_label(&die_combat, b.face), b.value);
}
```

```text output
loaded 6   combat Hit (1)
loaded 6   combat - (0)
loaded 6   combat Crit (2)
loaded 2   combat Hit (1)
loaded 3   combat - (0)
```

| Parameter | Meaning |
|---|---|
| `die` | the die to fill in |
| `faces` | the faces, in order. Face 0 is the first |
| `count` | how many: 1 to 30 (`QD_MAX_FACES`) |

**Returns** `false` (and leaves `die` alone) for 0 faces or more than 30.

**Weights.** A face's weight is how many tickets it has in the draw. The loaded die above has 8 tickets: a six is 3 of them (37.5%), every other face 1 (12.5%). The "combat" die is a six-sided die with three faces' worth of blank, written as one face of weight 3: the same odds, fewer faces to describe.

To roll several at once, with modifiers or counting: [`qd_roll_custom`](rolls.md#qd_roll_custom). In notation, [`d{name}`](../05-notation.md#custom-dice-dname).

---

## qd_die_fudge

A Fudge (FATE) die: two faces each of -1, 0 and +1, labelled "-", " " and "+".

```c
void qd_die_fudge(qd_die_t *die);
```

```c example=die_fudge
qd_die_t df;
qd_rng_init_hw();
qd_die_fudge(&df);
int total = 0;
for (int i = 0; i < 4; i++) {
    qd_face_roll_t r = qd_die_roll(&df);
    printf("[%s]", qd_die_face_label(&df, r.face));
    total += r.value;
}
printf("  = %+d\n", total);
```

```text output
[+][ ][+][-]  = +1
```

---

## qd_fudge_die

One shared, ready-made Fudge die: what `4dF` and the FATE system use. Saves declaring your own.

```c
const qd_die_t *qd_fudge_die(void);
```

```c example=fudge_die
qd_rng_init_hw();
qd_face_roll_t r = qd_die_roll(qd_fudge_die());
printf("%d\n", r.value);
```

```text output
1
```

**Returns** the die. It is made the first time it's asked for and lives for the whole program.

---

## qd_die_roll

Roll one die.

```c
qd_face_roll_t qd_die_roll(const qd_die_t *die);
```

```c example=die_roll
qd_die_t d20;
qd_rng_init_hw();
qd_die_standard(&d20, 20);
qd_face_roll_t r = qd_die_roll(&d20);
if (r.value == 20) printf("a natural 20!\n");
else printf("%d\n", r.value);
```

```text output
19
```

**Returns** the face that came up (0 to sides - 1) and its value. For a standard die the value is always face + 1.

---

## qd_die_face_value

The value of a face.

```c
int16_t qd_die_face_value(const qd_die_t *die, uint16_t face);
```

```c example=die_face_value
qd_die_t df;
qd_die_fudge(&df);
for (uint16_t f = 0; f < df.sides; f++)
    printf("face %u: %+d\n", f, qd_die_face_value(&df, f));
```

```text output
face 0: -1
face 1: -1
face 2: +0
face 3: +0
face 4: +1
face 5: +1
```

| Parameter | Meaning |
|---|---|
| `face` | 0 to sides - 1 |

---

## qd_die_face_label

The label of a face: the custom label, or `""` if it has none (standard dice never do).

```c
const char *qd_die_face_label(const qd_die_t *die, uint16_t face);
```

Example: [`qd_die_custom`](#qd_die_custom) above. A typical display shows the label if there is one, and the value if not:

```c example=die_face_label
qd_die_t df;
char shown[8];
qd_rng_init_hw();
qd_die_fudge(&df);
qd_face_roll_t r = qd_die_roll(&df);
const char *label = qd_die_face_label(&df, r.face);
if (*label) snprintf(shown, sizeof shown, "%s", label);
else snprintf(shown, sizeof shown, "%d", r.value);
printf("[%s]\n", shown);
```

```text output
[+]
```

---

## qd_die_face_ppm

The chance of a face, in parts per million: 1,000,000 is certain.

```c
uint32_t qd_die_face_ppm(const qd_die_t *die, uint16_t face);
```

```c example=die_face_ppm
static const qd_face_t loaded[] = {
    { 1, 1, "" }, { 2, 1, "" }, { 3, 1, "" }, { 4, 1, "" }, { 5, 1, "" }, { 6, 3, "" } };
qd_die_t die;
qd_die_custom(&die, loaded, 6);
printf("a one: %.1f%%, a six: %.1f%%\n", qd_die_face_ppm(&die, 0) / 1e4, qd_die_face_ppm(&die, 5) / 1e4);
```

```text output
a one: 12.5%, a six: 37.5%
```

Used by [stats for nerds](stats.md) to know what a fair (or fairly loaded) die should show.

---

## qd_roll_percentile

Roll d100 the way the dice do it: a units d10 and a tens d10, with Call of Cthulhu bonus and penalty dice.

```c
uint8_t qd_roll_percentile(int bonus, qd_percentile_t *out);

typedef struct {
    uint8_t units;            /* the units die, 0 .. 9 */
    uint8_t tens_count;       /* tens dice rolled: 1 + |bonus| */
    uint8_t tens[4];          /* each tens die, 0 .. 9 (read as 00 .. 90) */
    uint8_t kept;             /* which of tens[] counted */
    int8_t  bonus;            /* the bonus (+) or penalty (-) dice used */
    uint8_t result;           /* 1 .. 100 (00 and 0 read as 100) */
} qd_percentile_t;
```

```c example=roll_percentile
qd_percentile_t p;
qd_rng_init_hw();
qd_roll_percentile(2, &p);            /* two bonus dice: keep the best (lowest) tens */
printf("units %u, tens", p.units);
for (int i = 0; i < p.tens_count; i++)
    printf(i == p.kept ? " [%u0]" : " %u0", p.tens[i]);
printf("  -> %u\n", p.result);
```

```text output
units 9, tens 50 90 [20]  -> 29
```

| Parameter | Meaning |
|---|---|
| `bonus` | bonus dice (positive: keep the best result) or penalty dice (negative: keep the worst). They cancel, so pass bonus minus penalty. Clamped to -3 .. +3 |
| `out` | every die, which tens die counted, and the result. May be `NULL` |

**Returns** the result, 1 to 100.

**Reading the dice:** tens 30 and units 7 is 37; tens 00 and units 0 is 100, not 0. With bonus dice, extra tens dice are rolled and the one giving the lowest result counts (lower is better in percentile games); with penalty dice, the highest. A 00 tens die with a 0 units die is 100, the worst result, so a bonus die never picks it.

The same dice in a roll, with a skill to beat: [`qd_sys_coc`](systems.md#qd_sys_coc). In notation: [`d%`, `d% b2`](../05-notation.md#percentile-d).
