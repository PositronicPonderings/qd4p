# QD4P dice notation

This is the complete reference for the text QD4P understands: what you type
into the Dice Roller, what `qd_parse()` reads, and what `qd_format()` writes
back. Every example on this page is something you can type into
`qd4p_console` and see rolled.

QD4P's notation is inspired by Dice Maiden's, and most Dice Maiden rolls work
unchanged. Where the two differ, this page says so.

- [Quick reference](#quick-reference)
- [The order things happen in](#the-order-things-happen-in)
- [Dice](#dice)
- [Maths](#maths)
- [Advantage and disadvantage](#advantage-and-disadvantage)
- [Exploding dice](#exploding-dice)
- [Imploding dice](#imploding-dice)
- [Rerolls](#rerolls)
- [Keep and drop](#keep-and-drop)
- [Counting successes, failures and botches](#counting-successes-failures-and-botches)
- [Buckets](#buckets)
- [Group names](#group-names)
- [Labels, comments, repeats and several rolls](#labels-comments-repeats-and-several-rolls)
- [Spaces, capitals and the d1 question](#spaces-capitals-and-the-d1-question)
- [How results are shown](#how-results-are-shown)
- [Recipes for popular games](#recipes-for-popular-games)
- [Errors](#errors)
- [Warnings](#warnings)
- [Limits](#limits)
- [Differences from Dice Maiden](#differences-from-dice-maiden)
- [Grammar](#grammar)
- [Using the parser from C](#using-the-parser-from-c)

## Quick reference

| What | Write | Example | Means |
| --- | --- | --- | --- |
| Dice | `NdS` | `3d6` | three six-sided dice, added up |
| One die | `dS` | `d20` | the 1 can be left out |
| Percentile | `d%` | `d%` | a d100, 1 to 100 |
| Fudge / FATE | `NdF` | `4dF` | faces −1, −1, 0, 0, +1, +1 |
| Digit dice | `ddXY…` | `dd66` | a d6 for the tens, a d6 for the units: 11 to 66 |
| Custom die | `d{name}` | `2d{Loaded}` | a die the app defines |
| Maths | `+ - * / ( )` | `(1d8+2)*2` | usual order of operations |
| Advantage | `advN` | `1d20 adv1` | one extra die, set aside the lowest |
| Disadvantage | `disN` | `1d20 dis1` | one extra die, set aside the highest |
| Explode | `e` `eN` | `3d6 e6` | a 6 rolls one more die |
| Explode again | `ie` `ieN` | `1d6 ie` | …and that die can explode too |
| Implode | `i` `iN` | `1d10 i1` | a 1 rolls one more die and subtracts it |
| Reroll low | `rN` `irN` | `2d6 r1` | reroll 1s once (`ir`: until they aren't) |
| Reroll high | `rgN` `irgN` | `2d6 rg6` | reroll 6s once (`irg`: until they aren't) |
| Keep | `kN` `klN` `kmN` | `4d6 k3` | keep the highest, lowest or middle N |
| Drop | `dN` `dhN` | `4d6 d1` | drop the lowest or highest N |
| Successes | `tN` `tlN` | `6d10 t8` | each die 8+ (or 3−: `tl3`) is a success |
| Double | `dsN` | `5d10 t7 ds10` | 10s count as two successes |
| Failures | `fN` | `4d10 t8 f1` | each 1 takes a success away |
| Botches | `bN` | `5d10 t7 b1` | count 1s as botches |
| Buckets | `[…]` | `100d6 [<3, =6]` | count dice that pass tests |
| Group name | `name:` | `stress:2d6` | names a set of dice |
| Label | `(text)` | `(Bite) 1d20+5` | names the roll |
| Comment | `! text` | `1d20 ! flanking` | a note shown with the result |
| Repeat | `N x` | `6 x 4d6 k3` | roll it six times |
| Several rolls | `;` | `2d6 ; 1d20` | up to four rolls on one line |

## The order things happen in

Modifiers can be written in any order after the dice, but they always
happen in this order. This matters: it decides, for example, whether an
exploded die can be dropped.

1. **Roll the dice**, plus one extra die per level of advantage or
   disadvantage. Then **set aside** that many lowest dice (advantage) or
   highest dice (disadvantage). Set-aside dice are gone: they never explode,
   reroll or count.
2. **Explode, implode and reroll**, in the order you wrote them. Each one
   looks at every die still in play when its turn comes.
3. **Keep or drop**. This can keep or drop a die that came from an
   explosion.
4. **Add up** the kept dice, minus any imploded dice.
5. **Maths**: the totals of each set of dice, and the numbers, combine with
   `+ - * /` and brackets.
6. **Count** successes, failures, botches and buckets, over the kept dice.

So `10d6 e6 k8` explodes first and keeps the best eight afterwards, exploded
dice included, while `1d20 adv1 e20` sets one d20 aside before anything can
explode.

## Dice

### Standard dice: `NdS`

`N` dice with `S` sides each, showing 1 to `S`, added together.

```
3d6        three d6
d20        one d20 (the 1 is optional)
1d7        any number of sides: DCC's d3, d5, d7, d14, d16, d24, d30 all work
100d6      a hundred d6
```

- `N` is 1 to 1,000 (the whole roll can't have more than 1,000 dice).
- `S` is 1 to 32,767. A `d1` always shows 1 and earns a
  [warning](#warnings), but is allowed.
- There is no space between `N`, `d` and `S`: `4d6`, not `4 d6`.

### Percentile: `d%`

The same as `d100`: 1 to 100, every number equally likely. `qd_format`
writes it back as `1d100`.

Call of Cthulhu bonus and penalty dice (`d% b2`, `d% p1`) arrive with the
CoC game system in D5; until then the console's `pct` command rolls them.

### Fudge / FATE dice: `NdF`

Each die has six faces: −1, −1, 0, 0, +1, +1, shown as `[-]`, `[ ]` and
`[+]`. `4dF` is the usual FATE roll, −4 to +4.

```
4dF          a FATE roll
4dF + 2      with a skill of +2
4dF [=+]     and count the pluses
```

### Digit dice: `ddXY…`

Each digit is its own die, read as one number: the first die is the tens,
the next the units, and so on. Used for d66 tables (Year Zero, Traveller,
Warhammer) and similar.

```
dd66       a d6 then a d6: 11, 12 ... 16, 21 ... 66 (36 results, all equally likely)
dd666      three d6: 111 to 666 (216 results)
dd36       a d3 then a d6: 11 to 36
3dd66      three separate dd66 rolls, added up
5dd66 dh1  five, drop the highest
```

- Each digit's die has 1 to 9 sides. A `1` always shows 1 and earns a
  warning (`dd61` rolls 11, 21 … 61).
- Up to 4 digits.
- Digit dice can be kept, dropped, counted and put in buckets, but they
  can't explode, implode or reroll.

**`d66` is not `dd66`.** `d66` is one die numbered 1 to 66, just like `d20`
or `d100`. `dd66` is two six-sided dice read as digits. Some other dice
rollers use `d66` for the digit kind; QD4P keeps `dX` meaning "a number from
1 to X" everywhere.

### Custom dice: `d{name}`

A die defined by the app, with its own faces, values, weights and labels:
a loaded die, an average die (2, 3, 3, 4, 4, 5), a symbol die. The name goes
between braces and is up to 15 characters; spaces at either end are
ignored.

```
2d{Loaded}         two of the app's "Loaded" die
3d{custom} k2      in the console: the last die made with the 'custom' command
```

Where the names come from is up to the app (`qd_parse_set_lookup`). An
unknown name is an error that points at the name.

## Maths

Rolls combine with `+`, `-`, `*`, `/` and brackets, in the usual order:
brackets first, then `*` and `/`, then `+` and `-`, each left to right.

```
1d20 + 5             attack bonus
1d20 - 1d4           bane
1d20 - 1d4 * 2       1d20 - (1d4 * 2)
(1d8 + 2) * 2        a critical hit, doubled
8d6 / 2              half damage
-1d4                 a negative roll
2 * -1d6             minus signs work wherever a number could go
100 / 1d6 / 2        (100 / 1d6) / 2: left to right
```

- Numbers are whole numbers up to 1,000,000,000.
- **Division rounds toward zero**: 7 / 2 is 3, −7 / 2 is −3.
- Dividing by the number 0 is an error. Dividing by a roll that comes up 0
  gives 0 and the result is flagged.
- Totals that don't fit in 32 bits are clamped and flagged.
- `+` in front of something does nothing: `+5` is `5`.

## Advantage and disadvantage

`advN` rolls N extra dice and sets aside the N lowest. `disN` rolls N extra
and sets aside the N highest. The number is optional and means 1.

```
1d20 adv          D&D advantage: the better of two d20s
1d20 dis          disadvantage
1d20 adv1 + 5     with a bonus
3d6 adv2          roll 5d6, set aside the 2 lowest
2d6 ie6 adv1      roll 3d6, set aside the lowest, then explode what's left
```

- Advantage and disadvantage cancel: `adv2 dis1` is `adv1`.
- The setting aside happens **before** exploding (stage 1), so a set-aside
  die never explodes. Compare `k` and `d`, which happen after.
- Set-aside dice are shown in brackets, like dropped dice.

## Exploding dice

A die that explodes rolls one more die, which joins the pool and adds to
the total.

| Write | Explodes on | Can the new die explode? |
| --- | --- | --- |
| `e` | the highest face | no |
| `eN` | N or higher | no |
| `ie` | the highest face | yes, again and again |
| `ieN` | N or higher | yes, again and again |

```
3d6 e         each 6 adds one more d6
3d6 e5        each 5 or 6 adds one more d6
1d6 ie        a 6 adds another d6; if that's a 6 too, another...
4d10 t8 ie10  Chronicles of Darkness: 10-again
```

- "The highest face" means the die's top value: 6 on a d6, +1 on a Fudge
  die, the largest value on a custom die.
- One die can explode at most 100 times; the chain stops there and the
  result is flagged.
- `ie` that would explode on every face (`1d6 ie1`) is refused, because it
  could never stop.
- In the results, a die from an explosion is shown with `!`, right after
  the die that exploded: `6 !6 !3` is a 6 that exploded into a 6 that
  exploded into a 3.

## Imploding dice

A die that implodes rolls one more die, which is **subtracted** from the
total. The new die never joins the pool: it can't be kept, dropped, counted
or exploded. Implosion happens once (there is no "implode again").

| Write | Implodes on |
| --- | --- |
| `i` | the lowest face |
| `iN` | N or lower |

```
1d10 i1 e10    Cyberpunk Red: a 1 subtracts another d10, a 10 adds one
4d6 i1 e6 + 2  1s subtract a die, 6s add one, +2
```

The die that imploded still counts as itself (the 1 in Cyberpunk is still
a 1). The subtracted die is shown with `-`: `1 -7` is a 1 that cost 7 more.

## Rerolls

A die that is rerolled is set aside and a new die takes its place.

| Write | Rerolls | How often |
| --- | --- | --- |
| `r` | the lowest face | once |
| `rN` | N or lower | once |
| `ir` `irN` | as above | until it doesn't match |
| `rg` | the highest face | once |
| `rgN` | N or higher | once |
| `irg` `irgN` | as above | until it doesn't match |

```
2d6 r1        Great Weapon Fighting: reroll 1s once (keep the new roll)
2d6 r2        reroll 1s and 2s once
4d6 ir1 k3    ability scores where 1s are always rerolled
3d6 rg6       reroll 6s once
```

- The old die is shown struck out, `~1~`, followed by its replacement.
- `ir` and `irg` stop after 100 rerolls of one die (flagged), and are
  refused if they would match every face.
- "Reroll" in this section means the modifier. Rerolling chosen dice after
  the fact (bennies, pushes, luck) is a separate feature arriving in D6.

## Keep and drop

| Write | Keeps |
| --- | --- |
| `kN` | the N highest |
| `klN` | the N lowest |
| `kmN` | the N middle (the extra one, if any, is dropped from the top) |
| `dN` | all but the N lowest |
| `dhN` | all but the N highest |

The number is optional and means 1.

```
4d6 k3         ability scores: 4d6 drop the lowest
4d6 d1         the same thing written the other way
2d20 kl1       disadvantage written by hand
5d10 km3       the middle three of five
10d6 e6 k8     explode, then keep the best 8 (exploded dice included)
```

- One keep or drop per set of dice.
- Keeping more dice than there are keeps them all; `k0` keeps none and
  earns a warning.
- **Ties**: when dice tie, the lowest is the earliest rolled of them and the
  highest is the latest rolled. The total is the same either way; this just
  decides which die is shown as dropped, so it never changes from run to
  run.
- Dropped dice are shown in brackets: `(1)`.

## Counting successes, failures and botches

For dice pools where you count dice instead of adding them.

| Write | Each kept die that … | |
| --- | --- | --- |
| `tN` | is N or higher | is a success |
| `tlN` | is N or lower | is a success (roll-under games) |
| `dsN` | is N or higher (or lower, after `tl`) | counts as a second success |
| `ds` | is a success | counts twice |
| `fN` `f` | is N or lower (or the lowest face) | takes one success away |
| `bN` `b` | is N or lower (or the lowest face) | is counted as a botch |

```
6d10 t8           six dice, 8+ succeeds
5d10 t7 ds10      Exalted: 7+ succeeds, 10s count double
4d10 t8 f1        World of Darkness: 1s cancel successes
6d6 t5            Shadowrun: 5s and 6s
5d10 t6 b1        count successes and, separately, botches
6d10 tl3 ds       roll-under: 3 or less succeeds, and every success counts twice
```

- `t` and `tl` need a number. `ds` comes after them.
- The result shows **successes minus failures**, and the number of failures
  taken off. Botches are reported on their own and never change the
  successes.
- Counting looks only at kept dice: dropped, set-aside, rerolled and
  imploded dice never count.
- When a roll counts successes, the total is still worked out and shown,
  but the successes are the headline.
- In the results: `8*` a success, `10**` a double success, `1x` a failure,
  `b` a botch.

## Buckets

A bucket counts the kept dice that pass a test. Put a list of tests in
square brackets at the end of the roll, separated by commas. Dice that pass
none of them are counted as "other".

| Test | Matches |
| --- | --- |
| `<3` `<=3` `>4` `>=4` | less than, at most, more than, at least |
| `=6` or just `6` | exactly 6 |
| `!=1` | anything but 1 |
| `3-5` | 3, 4 or 5 |
| `=max` `=min` | the die's highest or lowest face |
| `=+` or `="+"` | a face labelled `+` (Fudge and custom dice) |
| `=-1` | negative numbers work |

```
100d6 [<3, =6]         how many low dice, how many sixes
10d10 [=max, =min]     natural 10s and 1s
4dF [=+, =-]           pluses and minuses (the labels; also [=1, =-1])
```

- Up to 8 buckets. A die can be in more than one bucket.
- Buckets are for your own categories. Built-in game outcomes (raises,
  criticals) come with the game systems in D5.

## Group names

A name and a colon in front of dice names that set, so a game system or an
app can tell them apart. Names are letters, digits and `_`, up to 7
characters.

```
base:4d6 t6 + stress:2d6 t6     Alien RPG: base dice and stress dice
hope:1d12 + fear:1d12           Daggerheart
```

The names are shown in the results and kept in each pool's `name`.

## Labels, comments, repeats and several rolls

**Label**: text in brackets at the start names the roll. It must start with
a letter, and is up to 23 characters.

```
(Bite) 1d20 + 14
(Sneak attack) 6d6
```

A bracket that starts with dice is maths, not a label: `(d6+1)*2` and
`(dF)` are maths, `(dodge) 1d20` is a label.

**Comment**: `!` and then anything to the end (or to the next `;`). Up to
47 characters are kept.

```
1d20 + 5 ! flanking the ogre
```

**Repeat**: a number and `x` in front rolls the same thing that many times,
1 to 20.

```
6 x 4d6 k3           six ability scores
3 x (Fire) 8d6       label and repeat go in either order
```

**Several rolls**: `;` separates up to four rolls on one line. Each has its
own label, comment and repeat.

```
2d6 ; 3d8 ; 1d20
(Hit) 1d20+7 ; (Damage) 2d6+4
```

## Spaces, capitals and the d1 question

Spaces are optional everywhere except inside a number or a dice term:
`4d6k3+2` and `4d6 k3 + 2` are the same roll. Keywords ignore case: `4D6K3`
works. Labels, comments, group names and custom die names keep their case.

Modifiers follow straight on from one another, so `ie` is one word
(explode again and again) while `i e` with a space is implode, then
explode. `qd_format` always writes a space between modifiers, so its output
is never ambiguous.

**After dice, `d` means drop.** `4d6d1` is "4d6, drop the lowest 1", and so
is `4d6 d1`. To add a one-sided die, use a sign: `4d6 + 1d1`. To add more
digit dice, likewise: `4d6 + dd66`.

## How results are shown

The console (and, in their own style, apps) shows every die:

| Shown | Means |
| --- | --- |
| `5` | kept |
| `(5)` | dropped, or set aside by advantage/disadvantage |
| `~2~` | rerolled: replaced by the die after it |
| `!6` | came from an explosion (printed after the die that exploded) |
| `-7` | imploded: subtracted from the total |
| `8*` `10**` | a success, a double success |
| `1x` | a failure (takes a success away) |
| `b` | a botch |
| `[+]` | a labelled face |
| `(cap)` | an explosion or reroll chain hit its limit here |

Over 100 dice, a roll switches to **summary mode**: the first 100 dice are
listed and every total, count and bucket stays exact. Face counts are kept
for dice with up to 30 faces.

## Recipes for popular games

| Game | Roll |
| --- | --- |
| D&D attack with advantage | `1d20 adv + 7` |
| D&D ability scores | `6 x 4d6 k3` |
| D&D Great Weapon Fighting | `2d6 r2 + 4` |
| D&D critical hit (2d8 doubled) | `(Crit) 4d8 + 4` |
| Pathfinder fortune/misfortune | `1d20 adv`, `1d20 dis` |
| FATE | `4dF + 2` |
| Chronicles of Darkness, 10-again | `7d10 t8 ie10` |
| CofD rote action | `7d10 t8 ie10 r7` |
| World of Darkness (old) | `6d10 t7 f1` |
| Exalted | `8d10 t7 ds10` |
| Shadowrun 6 | `12d6 t5` |
| Year Zero (Alien) | `base:4d6 t6 + stress:2d6 t6` |
| Warhammer 40k to-hit | `10d6 t4` |
| Cyberpunk Red | `1d10 i1 e10 + 12` |
| Dark Heresy righteous fury | `4d10 ie10` |
| Storypath | `6d10 t8 ie10` |
| d66 table | `dd66` |
| Hero System normal damage | `6d6 [=1, =6]` (1 = 0 BODY, 6 = 2 BODY) |

Game systems in D5 add shortcuts and outcomes (raises, success levels,
messy criticals) for D&D 5e, Savage Worlds, Call of Cthulhu 7e, FATE,
Vampire 5e and DCC.

## Errors

The parser stops at the first problem and reports the column and what's
wrong. The console underlines it:

```
> 4d6 k3 +
  4d6 k3 +
          ^ the roll stops too soon: a number, dice or '(' should come next
```

| Message | Typical cause |
| --- | --- |
| nothing to roll | an empty line, or only a label |
| the roll stops too soon … | a sign with nothing after it: `4d6 +` |
| expected a number, dice or '(' … | something odd where a value should be |
| expected + - * / [ ! or the end … | something odd after a complete roll |
| no space inside dice | `4 d6` |
| what kind of die? | `4d` or `4dx` |
| a die needs at least 1 side / at most 32767 sides | `4d0`, `4d40000` |
| at most 1000 dice in a roll | `1001d6` |
| '…' isn't a modifier | a word the parser doesn't know |
| t needs a number | `6d10 t` |
| ds goes after t or tl | `6d10 ds10` |
| only one keep or drop per dice | `4d6 k3 k2` |
| at most 4 explode, implode and reroll steps | `1d6 e e e e e` |
| more dice need a + or - | `4d6 dd66` |
| digit dice have 1 to 9 sides, not 0 | `dd60` |
| digit dice can't explode, implode or reroll | `2dd66 e` |
| no custom die called '…' | `2d{nope}` |
| missing ')' / '}' / ']' / '"' | an unclosed bracket, brace or quote |
| a ')' without its '(' | `1d6+2)` |
| can't divide by zero | `1d6 / 0` |
| no dice in this roll | `3 + 4` |
| a group name goes before dice | `base:5` |
| repeat 1 to 20 times | `25 x 1d6` |
| a label is at most 23 characters | |
| at most 8 sets of dice in one roll | nine dice terms |
| at most 8 buckets | |
| brackets nested too deep | more than 8 brackets inside each other |
| too long: at most 24 numbers, dice and signs | a very long sum |
| explodes or rerolls on every face, so it would never stop | `1d6 ie1` |
| at most 4 rolls on one line | a fifth `;` |

## Warnings

Some rolls are allowed but probably not what was meant. They roll, with a
note:

| Warning | Example |
| --- | --- |
| a die with one side always shows the same number | `3d1`, `dd61` |
| keeping 0 dice: the total is always 0 | `4d6 k0` |

## Limits

All are compile-time settings in `qd4p/qd_config.h`.

| Limit | Default |
| --- | --- |
| Dice in one roll, explosions included | 1,000 |
| Dice listed one by one (past this: summary mode) | 100 |
| Explosions or rerolls from one die | 100 |
| Sides on a standard die | 32,767 |
| Digits in digit dice | 4 |
| Sets of dice in one roll | 8 |
| Explode, implode and reroll steps per set | 4 |
| Numbers, dice and signs in one roll's sum | 24 |
| Buckets | 8 |
| Brackets inside brackets | 8 |
| Repeats (`N x`) | 20 |
| Rolls on one line (`;`) | 4 |
| Label / comment / group name / custom die name | 23 / 47 / 7 / 15 characters |

## Differences from Dice Maiden

- `d66` is a 1–66 die; digit dice are `dd66`.
- Dice Maiden's game aliases (`sw8`, `4cod`, `vtm7h2` …) are not notation
  here; QD4P's game systems (D5) are their own layer with their own
  shortcuts.
- Roll sets are written `6 x 4d6 k3` (Dice Maiden: `6 4d6 k3`).
- Buckets (`[<3, =6]`), group names (`base:`), custom dice (`d{name}`) and
  digit dice are QD4P's own.
- Maths uses the usual precedence and allows brackets anywhere.
- The private/simple/unsorted flags (`p`, `s`, `nr`, `ul`) are display
  choices for the app, not part of the notation.

## Grammar

For the curious, and for anyone writing a compatible parser:

```
line     = roll { ";" roll }
roll     = { repeat | label } sum [ buckets ] [ "!" comment ]
repeat   = NUMBER "x"
label    = "(" LETTER { any but ")" } ")"
sum      = product { ( "+" | "-" ) product }
product  = unary { ( "*" | "/" ) unary }
unary    = "-" unary | "+" unary | primary
primary  = NUMBER | dice | "(" sum ")"
dice     = [ NAME ":" ] [ NUMBER ] "d" kind { modifier }
kind     = NUMBER | "%" | "F" | "{" NAME "}" | "d" DIGIT { DIGIT }
modifier = ( "adv" | "dis" | "e" | "ie" | "i" | "r" | "ir" | "rg" | "irg"
           | "k" | "kl" | "km" | "d" | "dh" | "t" | "tl" | "ds" | "f" | "b" ) [ NUMBER ]
buckets  = "[" bucket { "," bucket } "]"
bucket   = [ "<" | "<=" | ">" | ">=" | "=" | "!=" ] ( INTEGER | INTEGER "-" INTEGER
           | "max" | "min" | LABEL | '"' LABEL '"' )
```

Keywords are matched longest first (`irg` before `ir` before `i`), and are
case-insensitive. Spaces may appear between any two symbols above.

## Using the parser from C

```c
#include "qd4p.h"

qd_parsed_t roll;
qd_parse_error_t err;
qd_result_t res;

if (!qd_parse("(Bite) 1d20 + 14", &roll, &err)) {
    printf("column %d: %s\n", err.pos + 1, err.message);   /* underline err.len chars */
    return;
}
for (int i = 0; i < roll.repeat; i++) {
    qd_roll_run(&roll.roll, &res);              /* parse once, roll many times */
    printf("%s: %ld\n", roll.label, (long)res.total);
}
```

- `qd_parse_line(text, rolls, max, &err)` reads several rolls separated by
  `;` and returns how many.
- `qd_format(&roll, buf, size)` writes a parsed roll back as tidy notation;
  parsing that gives the same roll. `qd_format_pool` writes one set of
  dice.
- `qd_parse_set_lookup(fn, ctx)` supplies custom dice for `d{name}`.
- `qd_roll_warnings(&roll.roll)` and `qd_warn_text()` give the warnings.
- Everything a parsed roll contains can also be built directly in C with
  the `qd_roll_*` and `qd_pool_*` functions in `qd_roll.h`; the parser is
  optional.
