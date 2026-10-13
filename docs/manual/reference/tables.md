# Random tables (the example)

[open](#tbl_open) · [find](#tbl_find) · [name, dice, entries](#tbl_name) · [roll](#tbl_roll) · [the result](#tbl_result_t)

Header: `examples/tables/table.h`. **Example code, not part of the library**: copy it into your app and change what you like. The whole guide, with the text format and the PC tool, is [`examples/tables/README.md`](../../../examples/tables/README.md).

**The example table system, in three lines:**
1. Tables are written as text on a PC, the way rulebooks print them, and `tables.py` turns them into a small binary **asset** (or a C array).
2. On the device, `tbl_roll` rolls on a table: its dice, `{dice}` in the entry, `[links]` to other tables, "Roll twice".
3. It keeps the **working**, every roll it made, so players can see how the result came about.

It is built from [`qd_roll_text`](text.md#qd_roll_text) and [`qd_expand_ex`](text.md#qd_expand_ex), so it brings in the parser ([what it costs](../../SIZES.md)).

The examples here use the console's six sample tables (`examples/tables/sample_tables.txt`), built into the array `sample_tables[]`.

---

## tbl_open

Open an asset: check it once, and keep a pointer to it.

```c
bool tbl_open(tbl_set_t *t, const void *data, uint32_t size);
```

```c example=tbl_open tables
tbl_set_t tables;
if (tbl_open(&tables, sample_tables, sizeof sample_tables))
    printf("%d tables\n", tables.count);
```

```text output
6 tables
```

| Parameter | Meaning |
|---|---|
| `t` | the set: 12 bytes, a pointer and a count |
| `data`, `size` | the asset. **Not copied**: it is read where it lies, in flash or an asset pack |

**Returns** `false` if the asset is damaged: every offset and string is checked once, so a damaged asset is refused rather than read past its end.

---

## tbl_find

A table's number, from its name (any case).

```c
int tbl_find(const tbl_set_t *t, const char *name);
```

**Returns** the number (0 is the first table), or -1 if there's no such table.

---

## tbl_name

A table's name, its dice, and how many entries it has.

```c
const char *tbl_name(const tbl_set_t *t, int index);
const char *tbl_dice(const tbl_set_t *t, int index);
int tbl_entries(const tbl_set_t *t, int index);
```

```c example=tbl_name tables
tbl_set_t tables;
tbl_open(&tables, sample_tables, sizeof sample_tables);
for (int k = 0; k < tables.count; k++)
    printf("%d  %-18s %-5s %d entries\n", k + 1, tbl_name(&tables, k), tbl_dice(&tables, k),
           tbl_entries(&tables, k));
```

```text output
1  Forest encounters  2d6   5 entries
2  Bandit leaders     1d4   3 entries
3  Weather            d6    4 entries
4  Treasure           d%    5 entries
5  Trinket            dd66  4 entries
6  Reaction           2d6   5 entries
```

---

## tbl_roll

Roll on a table, with the working.

```c
bool tbl_roll(const tbl_set_t *t, int index, tbl_result_t *out);
```

```c example=tbl_roll tables
static tbl_set_t tables;
static tbl_result_t r;
qd_rng_init_hw();
tbl_open(&tables, sample_tables, sizeof sample_tables);
for (int n = 0; n < 3; n++) {
    tbl_roll(&tables, tbl_find(&tables, "Forest encounters"), &r);
    printf("%s\n", r.text);
    for (int i = 0; i < r.steps; i++)
        printf("  %*s%s %s: %ld\n", 2 * r.step[i].depth, "",
               r.step[i].kind == TBL_STEP_INLINE ? "" : tbl_name(&tables, r.step[i].table),
               r.step[i].what, (long)r.step[i].value);
}
```

```text output
Bandits, led by the twins Ada and Ilse, who finish each other's threats
  Forest encounters 2d6: 10
    Bandit leaders 1d4: 4
Nothing stirs. Thunder in the distance; the wind is rising from the north.
  Forest encounters 2d6: 6
    Weather d6: 6
Nothing stirs. A light drizzle.
  Forest encounters 2d6: 7
    Weather d6: 2
```

**Returns** `false` for a table number that doesn't exist.

---

## tbl_result_t

The result: the text, flags, and the working.

```c
typedef struct {
    char       text[321];        /* the finished text (TBL_TEXT) */
    uint16_t   flags;            /* TBL_CUT, TBL_DEEP, TBL_NO_ROW, TBL_BAD, TBL_STEPS_FULL */
    uint8_t    steps;
    tbl_step_t step[24];         /* every roll made (TBL_STEPS) */
} tbl_result_t;
```

Each step is a table roll (`TBL_STEP_TABLE`), an extra roll from "Roll twice" (`TBL_STEP_AGAIN`) or a `{roll}` in an entry (`TBL_STEP_INLINE`), with its `table`, the dice as written (`what`), the `value`, and how deep in the links it was (`depth`). A `tbl_result_t` is about 1.1 KB: make it `static`.

| Flag | Means |
|---|---|
| `TBL_NO_ROW` | a roll fell in no entry: shown as "(no entry for 13)" |
| `TBL_DEEP` | links went deeper than `TBL_DEPTH` (4): shown as "..." |
| `TBL_BAD` | a link or roll in an entry couldn't be read: shown as `[?]` or `{?}` |
| `TBL_CUT` | the text was longer than 320 characters, and was cut |
| `TBL_STEPS_FULL` | more than 24 rolls: the rest were made but not kept in the working |
