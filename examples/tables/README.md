# Random tables: an example

**This is example code, not part of the QD4P library.** QD4P provides the
building blocks every table system needs (`qd4p/qd_text.h`: dice inside
text, a hook for links, rolls that pick from ranges); this folder is one
complete table system built from them, with its own file format and PC
tool. Copy it into an app and change it to suit: columns, weights,
no-repeat draws, conditions. The console uses it for its sample tables.

| File | What it is |
| --- | --- |
| `table.h`, `table.c` | the device side: read the asset in place, roll with the working (`tbl_*`) |
| `tables.py` | the PC side: check, build, dump |
| `sample_tables.txt` | six sample tables |
| `sample_tables.h` | the same, built as a C array for the console |

A random table is a die roll and a list of results, the way rulebooks print
them. This system rolls them on the device, following links to other
tables and rolling any dice inside an entry, and keeps the working so you
can see what was rolled. Tables are written as plain text on a PC, checked
and turned into a small binary asset by `tables.py`, and read by the
device straight from flash.

## Writing a table

```
# Lines starting with # are comments.

table Forest encounters
roll 2d6
2      An owlbear, very hungry
3-4    {1d4+1} wolves, circling
5-9    Nothing stirs. [Weather]
10-11  Bandits, led by [Bandit leaders]
12     Roll twice

table Bandit leaders
1      Grey Meg, one-eyed and patient
2      Tomas the Smiler, who never stops talking
3-4    the twins Ada and Ilse, who finish each other's threats
```

- **`table <name>`** starts a table. Names can have spaces and are matched
  in any case. Each name must be different.
- **`roll <dice>`** (optional) is any [dice notation](notation.md): `2d6`,
  `d%`, `dd66`, `1d20 + 2`, even `coc 50`. Leave it out and the table rolls
  1dN, where N is its highest range: the `Bandit leaders` table above rolls
  a d4.
- **Entries**: a range, then the text. A range is:

  | Write | Means |
  | --- | --- |
  | `7` | exactly 7 |
  | `3-4` | 3 or 4 |
  | `13+` | 13 or more (for rolls with modifiers) |
  | `2-` | 2 or less |
  | `00` | 100 (percentile tables: `01-40`, ..., `00`) |

- **Long entries**: a line starting with a space or tab carries on the entry
  above it.

### Live parts of an entry

| Write | Does |
| --- | --- |
| `{1d4+1}` | rolls the dice each time and puts the number in: "3 wolves" |
| `[Weather]` | rolls on the Weather table and puts its result in |
| `Roll twice` | (the whole entry) rolls twice more on this table and gives both results, separated by "; " |
| `Roll 3 times`, `Roll thrice` | likewise, three times (up to 9) |
| `\{` `\[` `\\` | a literal `{`, `[` or `\` |

- Any notation works inside `{ }`: `{2d6 * 10}`, `{4dF}`, `{6d10 t8}`
  (successes).
- "Roll twice" ignores itself: if a roll-again entry comes up during the
  extra rolls, that roll is made again.
- Links can go into links (`[Treasure]` -> `[Trinket]`) up to 8 deep; a
  table that links back to itself stops there with "...".

## Checking and building

```
python3 examples/tables/tables.py check  my_tables.txt
python3 examples/tables/tables.py build  my_tables.txt -o tables.bin
python3 examples/tables/tables.py build  my_tables.txt --c my_tables.h --name my_tables
python3 examples/tables/tables.py dump   tables.bin > recovered.txt
```

`check` (and `build`, which checks first) reports:

| Error (nothing is built) | Warning (built anyway) |
| --- | --- |
| two tables with the same name | a value the roll can make has no entry (simple rolls: `2d6`, `d20`, `d%`, `3d6+2`, `dd66` ...) |
| ranges that overlap | |
| a `[link]` to a table that doesn't exist | |
| a `{` without `}`, or an empty `{}` | |
| a line that isn't a table, roll or entry | |
| no `roll` line and ranges that don't start at 1 | |

`dump` turns an asset back into text, so tables on a device can always be
recovered and edited.

`--c` writes the asset as a C array, for building tables into a program
(the console's samples are built this way, from `sample_tables.txt`).

## On the device

```c
#include "table.h"

tbl_set_t tables;
tbl_result_t r;

tbl_open(&tables, asset, asset_size);                  /* checks every byte once */
tbl_roll(&tables, tbl_find(&tables, "Forest encounters"), &r);

printf("%s\n", r.text);                                /* "Bandits, led by Grey Meg, ..." */
for (int i = 0; i < r.steps; i++)                      /* the working */
    printf("%*s%s %s: %ld\n", 2 * r.step[i].depth, "",
           tbl_name(&tables, r.step[i].table), r.step[i].what, (long)r.step[i].value);
```

- **The working** (`r.step[]`): every table roll (which table, the dice,
  the value), every inline roll, and every extra roll from "roll twice",
  with how deep in the links it happened.
- **Flags** in `r.flags`: `TBL_NO_ROW` (a value with no entry, shown as
  "(no entry for 13)"), `TBL_DEEP` (links too deep), `TBL_BAD` (an entry's
  link or roll couldn't be read, shown as `[?]` or `{?}`), `TBL_CUT` (text
  longer than `TBL_TEXT`, 320 characters).
- The asset is read where it lies, never copied, so it can live in flash
  or an asset pack. `tbl_open` checks every offset and string once, so a
  damaged asset is refused rather than read past its end.
- Limits (`table.h`): `TBL_DEPTH` 8, `TBL_TEXT` 320, `TBL_STEPS` 24.
- How it uses QD4P: `qd_roll_text` rolls each table's dice, and
  `qd_expand_ex` expands each entry, with a hook that rolls `[links]` on
  the named table. The rest (the asset, "Roll twice", the working) is this
  example's own.

In the console: `tables` lists the samples, `table forest` (a name, the
start of one, or a number) rolls one with its working.

## The asset format

All of it is this example's choice; an app is free to store tables any way
it likes.

Little-endian; every string ends in a 0 byte so the device can use it in
place.

```
"QDT1"   u16 table count   u16 0   u32 offset of each table
table:   name\0   dice\0   u16 entry count   entries
entry:   i32 low   i32 high   u8 kind (0 text, 1 roll again)   u8 times   text\0
```

Open-ended ranges use the int32 limits: `13+` is 13 to 2147483647.
