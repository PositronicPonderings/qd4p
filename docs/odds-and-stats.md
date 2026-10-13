# Odds and stats for nerds

Two ways QD4P answers "how likely is that?": **odds** work out the chances
before you roll, and **stats for nerds** count what actually happened.

## Odds

```c
qd_parsed_t p;
qd_odds_t odds;
qd_parse("atk adv + 5 vs 15", &p, &err);
qd_odds(&p.roll, &odds);
/* odds.pass 0.7975; odds.level[QD_LVL_CRIT + 4] 0.0975 (critical hits);
   odds.mean 18.825; odds.dist[] the chance of each total from odds.first */
```

In the console: `odds 4d6k3`, `odds atk adv + 5 vs 15`, `odds coc 65 b1`.

### What you get

| Field | Means |
| --- | --- |
| `mean`, `sd` | the average value and its spread |
| `lo`, `hi` | the lowest and highest value possible (simulated: seen) |
| `dist[]`, `first`, `bins` | the chance of each value, from `first` |
| `qd_odds_at_least(&o, v)` | the chance of `v` or more |
| `qd_odds_exactly(&o, v)` | the chance of exactly `v` |
| `judged`, `pass` | with a target (`vs`) or a game system: the chance of passing |
| `level[L + 4]` | the chance of each outcome level; `qd_sys_level_name` names them |
| `counting` | the value is successes rather than the total |
| `exact`, `method`, `runs` | how it was worked out (below) |

The value is the total, or the successes for a roll that counts them. For a
repeat or a set (`6 x 4d6k3`) the odds are for one roll.

### How

QD4P picks the first of three methods that fits:

1. **Exact, die by die** (`QD_ODDS_EXACT_SUM`). Sums and success counts of
   plain dice (no keep or drop, advantage, exploding or rerolls), with
   `+ - *` and numbers: `3d6`, `1d20 + 5 vs 15`, `6d10 t8 f1`,
   `(1d8 + 2) * 2`, `100d6`. Each die's chances are combined in turn
   (convolution). Fast, even for a hundred dice, as long as the totals
   span at most `QD_ODDS_SPAN` (1,024) values. Chances are kept in single
   precision, accurate to about one part in a million.
2. **Exact, every roll** (`QD_ODDS_EXACT_ALL`). Anything else small
   enough: every possible roll is tried once and weighted by its chance.
   `4d6k3` is 1,296 rolls, `atk adv` 400, `coc 65 b1` 1,000, `fate` 1,296,
   `5d6 k3` 7,776. Up to `QD_ODDS_EXACT_RUNS` (50,000).
3. **Simulated** (`QD_ODDS_SIMULATED`). Everything else: `QD_ODDS_SAMPLES`
   (10,000) rolls, fewer for very big rolls (`QD_ODDS_DICE_BUDGET`, 2
   million dice). Exploding dice that can explode again (`ie`, so Savage
   Worlds) and big pools (`vtm 8 h3`) land here. The answer is close, not
   exact: with 10,000 rolls a chance near 50% is usually within a
   percentage point.

Working out odds never changes the rolls that follow: the generator is put
back exactly as it was (replay mode gives the same rolls as if you hadn't
asked), and none of the dice are counted in stats.

## Stats for nerds

```c
static qd_stats_t stats;
qd_stats_clear(&stats);
qd_stats_watch(&stats);              /* from now on every die is counted */
...
qd_stats_report_t r;
qd_stats_report(qd_stats_find(&stats, 20, NULL), &r);
/* r.mean 10.62, r.expect_mean 10.5, r.highest (natural 20s), r.verdict */
```

In the console: `stats`, `stats 20` (a d20's faces as bars), `stats clear`.
The console counts from the moment it starts.

### What is counted

Every die the roll engine rolls: kept, dropped, set aside, exploded and
rerolled alike, because each is a real roll of that die. Percentile dice
count as the two (or more) d10s they are, with 0 read as 10, and digit
dice as one die per digit (`dd66` is two d6s). Dice are kept apart by
their sides, and custom dice by which die they are: a loaded d6 is judged
as a loaded d6. Up to `QD_STATS_KINDS` (16) kinds; faces are counted one by
one for dice up to `QD_STATS_FACES` (100) sides.

### The fairness check

A chi-square test: how far the face counts are from what a fair die (or,
for a weighted custom die, a correctly weighted one) would give. The
verdict:

| Verdict | Means |
| --- | --- |
| `QD_STATS_TOO_FEW` | fewer than about 5 rolls per face so far (30 for a d6, 100 for a d20) |
| `QD_STATS_FAIR` | behaving |
| `QD_STATS_ODD` | counts this uneven happen 1 time in 1,000 with a fair die |
| `QD_STATS_NO_FACES` | too many sides to check face by face; average and spread only |

One "odd" is a curiosity: with enough kinds of dice and enough checks,
fair dice will be odd now and then. The same die odd again and again,
after clearing, is worth a look. The limits are the standard table values
(p = 0.001) up to a d31, then a close approximation (`qd_chi_limit`).

The report also gives the average and spread against a fair die's, and
how many times the top and bottom faces came up: natural 20s and natural
1s.
