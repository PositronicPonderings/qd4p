# Dice in text and ranges

[roll text](#qd_roll_text) · [roll n characters](#qd_roll_textn) · [ranges](#qd_range_t) · [find a range](#qd_range_find) · [pick a range](#qd_pick_range) · [dice in text](#qd_expand) · [your own brackets](#qd_expand_ex) · [the working](#qd_expand_info_t)

Header: `qd_text.h` (included by `qd4p.h`).

**The building blocks of random tables, in three lines:**
1. **Dice in text:** `"{1d4+1} wolves and {2d6*10} gp"` becomes `"3 wolves and 70 gp"`.
2. **Ranges:** roll, then find which of your ranges the value falls in, the way a rulebook table works (2: dragon, 3-4: wolves, ...).
3. QD4P doesn't say what a random table *is*, because every app wants something different: columns, weights, no repeats, pictures. A complete table system built from these pieces is in `examples/tables` ([reference](tables.md)), to use as it is or to start from.

Everything here except [`qd_range_find`](#qd_range_find) reads notation, so it brings in the parser ([what it costs](../../SIZES.md)). A table with fixed dice can roll them in code and use `qd_range_find` alone.

---

## qd_roll_text

Roll notation written as text, and get its value: the total, or the successes for a roll that counts them.

```c
bool qd_roll_text(const char *notation, int32_t *value);
```

```c example=roll_text
int32_t v;
qd_rng_init_hw();
if (qd_roll_text("3d6 + 2", &v)) printf("3d6 + 2: %ld\n", (long)v);
if (qd_roll_text("6d10 t8", &v)) printf("6d10 t8: %ld successes\n", (long)v);
if (!qd_roll_text("three dice", &v)) printf("'three dice' isn't a roll\n");
```

```text output
3d6 + 2: 18
6d10 t8: 2 successes
'three dice' isn't a roll
```

**Returns** `false` if the text isn't one roll (or is over 63 characters). For the dice, the label or the outcome, use [`qd_parse`](parse.md#qd_parse) and [`qd_roll_run`](rolls.md#qd_roll_run) instead.

---

## qd_roll_textn

The same for text that isn't zero-terminated: the first `n` characters.

```c
bool qd_roll_textn(const char *notation, int n, int32_t *value);
```

```c example=roll_textn
const char *line = "2d6+1|Goblins";
int32_t v;
qd_rng_init_hw();
int n = (int)(strchr(line, '|') - line);
if (qd_roll_textn(line, n, &v)) printf("%ld %s\n", (long)v, line + n + 1);
```

```text output
11 Goblins
```

---

## qd_range_t

A range of values, both ends included. For an open end ("13 or more"), use `INT32_MIN` or `INT32_MAX` (from `<stdint.h>`).

```c
typedef struct { int32_t lo, hi; } qd_range_t;
```

---

## qd_range_find

Which range a value falls in.

```c
int qd_range_find(const qd_range_t *ranges, int n, int32_t v);
```

```c example=range_find
static const qd_range_t reaction[] = { { 2, 2 }, { 3, 5 }, { 6, 8 }, { 9, 11 }, { 12, 12 } };
static const char *const mood[] = { "Hostile", "Unfriendly", "Unsure", "Talkative", "Helpful" };
for (int roll = 2; roll <= 12; roll += 3)
    printf("%2d: %s\n", roll, mood[qd_range_find(reaction, 5, roll)]);
```

```text output
 2: Hostile
 5: Unfriendly
 8: Unsure
11: Talkative
```

**Returns** the index of the first range that holds `v`, or -1 for none. No parser: this is all a table with fixed dice needs.

---

## qd_pick_range

Roll notation and find its range, in one go.

```c
int qd_pick_range(const char *notation, const qd_range_t *ranges, int n, int32_t *value);
```

```c example=pick_range
static const qd_range_t weather[] = { { 1, 2 }, { 3, 4 }, { 5, 5 }, { 6, 6 } };
static const char *const sky[] = { "Clear", "Cloudy", "Rain", "Storm" };
int32_t v;
qd_rng_init_hw();
for (int day = 1; day <= 4; day++) {
    int row = qd_pick_range("1d6", weather, 4, &v);
    printf("day %d: rolled %ld, %s\n", day, (long)v, row >= 0 ? sky[row] : "?");
}
```

```text output
day 1: rolled 6, Storm
day 2: rolled 4, Cloudy
day 3: rolled 6, Storm
day 4: rolled 2, Clear
```

**Returns** the index, -1 if the value falls in no range, or -2 if the notation can't be read. The value rolled goes in `*value` either way (0 if unreadable).

---

## qd_expand

Copy text, rolling each `{notation}` in it and putting the value in its place.

```c
int qd_expand(const char *text, char *out, int size, qd_expand_info_t *info);
```

```c example=expand
char line[80];
qd_rng_init_hw();
qd_expand("{1d4+1} wolves guard {2d6*10} gp and \\{ a brace }", line, sizeof line, NULL);
printf("%s\n", line);
qd_expand("{1d6} trolls and {lots} of goblins", line, sizeof line, NULL);
printf("%s\n", line);
```

```text output
5 wolves guard 100 gp and { a brace }
2 trolls and {?} of goblins
```

| Parameter | Meaning |
|---|---|
| `text` | the text. `\{`, `\[` and `\\` give a literal `{`, `[` or `\` |
| `out`, `size` | where the result goes; always zero-terminated |
| `info` | the working: each roll and its value, and flags. May be `NULL` |

**Returns** the length written. A `{...}` that isn't a roll becomes `{?}` (and sets `QD_EXPAND_BAD`); a result too long for `out` is cut (`QD_EXPAND_CUT`). `[brackets]` are left as they are: [`qd_expand_ex`](#qd_expand_ex) is for giving them a meaning.

---

## qd_expand_ex

`qd_expand`, with a **hook** that decides what `[brackets]` mean. A random-table app uses it for `[Table]` links; a name generator could use it for `[Name]`.

```c
typedef int (*qd_expand_hook_t)(const char *name, int n, char *out, int room, void *ctx);
int qd_expand_ex(const char *text, char *out, int size, qd_expand_info_t *info,
                 qd_expand_hook_t hook, void *ctx);
```

```c example=expand_ex
static int names(const char *name, int n, char *out, int room, void *ctx)
{
    static const char *const first[] = { "Anya", "Bram", "Corin", "Dell" };
    if (n == 4 && !strncmp(name, "Name", 4))
        return snprintf(out, (size_t)room + 1, "%s", first[qd_rng_range(4)]);
    return -1;                                     /* not ours: leave the brackets */
}

int main(void)
{
    char line[80];
    qd_rng_init_hw();
    qd_expand_ex("[Name] and [Name] owe you {2d6} silver. [Other] stays.", line, sizeof line,
                 NULL, names, NULL);
    printf("%s\n", line);
    return 0;
}
```

```text output
Dell and Corin owe you 8 silver. [Other] stays.
```

The hook is called with the text between `[` and `]` (`n` characters, not zero-terminated). It writes its replacement into `out` (at most `room` characters) and returns how many it wrote, or -1 to leave the brackets as they are. The hook may call `qd_expand_ex` itself: that's how a table links to a table.

---

## qd_expand_info_t

What `qd_expand` did: its flags, and every `{roll}` with its value. Show it as "the working".

```c
typedef struct {
    uint16_t flags;            /* QD_EXPAND_CUT, _BAD, _FULL */
    uint8_t  rolls;
    struct { int32_t value; char what[24]; } roll[16];   /* QD_EXPAND_ROLLS */
} qd_expand_info_t;
```

```c example=expand_info
char line[80];
qd_expand_info_t info;
qd_rng_init_hw();
qd_expand("{2d6} bandits with {1d4} wagons", line, sizeof line, &info);
printf("%s\n", line);
for (int i = 0; i < info.rolls; i++) printf("  {%s}: %ld\n", info.roll[i].what, (long)info.roll[i].value);
```

```text output
10 bandits with 4 wagons
  {2d6}: 10
  {1d4}: 4
```

| Flag | Means |
|---|---|
| `QD_EXPAND_CUT` | the result didn't fit in `out`, and was cut |
| `QD_EXPAND_BAD` | a `{...}` couldn't be read: it shows as `{?}` |
| `QD_EXPAND_FULL` | more than 16 rolls: the rest were rolled but not kept here |
