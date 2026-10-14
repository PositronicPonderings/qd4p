# Too busy to read a manual

Everything, on one page. Every function exists exactly as written here; details are one click away in the [reference](reference/README.md). Include `qd4p.h` for all of it.

**Types, so the lines below stay short:** `r` is a `qd_roll_t *`, `p` a `qd_pool_t *`, `res` a `qd_result_t *`, `t` a `qd_test_t` (by value). `→ err` returns `qd_err_t` (`QD_OK` on success).

## Start

```c example=quickref
static qd_roll_t roll;                              /* static: too big for the stack */
static qd_result_t res;
qd_rng_init_hw();                                   /* once, at boot */
qd_roll_init(&roll);                                /* build once ... */
qd_pool_keep(qd_roll_dice(&roll, 4, 6), QD_KEEP_HIGH, 3);
qd_roll_run(&roll, &res);                           /* ... run whenever */
printf("4d6 k3: %ld\n", (long)res.total);
```

```text output
4d6 k3: 16
```

## Randomness ([more](reference/randomness.md))

```c
qd_rng_init_hw()                              /* seed from the hardware; leave replay */
qd_rng_between(lo, hi)        → int32_t       /* lo..hi, fair */
qd_rng_range(n)               → uint32_t      /* 0..n-1, fair */
qd_rng_next32()               → uint32_t      /* raw bits */
qd_rng_seed_fixed(seed)                       /* replay mode: same rolls every time */
qd_rng_is_replay()            → bool
qd_rng_stir(bytes, len)       → bool          /* mix in anything; can only help */
qd_rng_stir32(v)              → bool
qd_rng_stir_count()           → uint32_t
qd_rng_save(state[4])   qd_rng_restore(state[4])
qd_rng_stir_source(&src, samples) → uint32_t  /* readings from a qd_entropy_source_t */
qd_entropy_trng   qd_entropy_adc(&a, gpio)   qd_entropy_event()   /* Pico: qd_entropy_pico.h */
```

## Dice and decks ([dice](reference/dice.md), [decks](reference/decks.md))

```c
qd_die_standard(&die, sides)  → bool          qd_die_custom(&die, faces, n) → bool
qd_die_fudge(&die)            qd_fudge_die()  → const qd_die_t *
qd_die_roll(&die)             → qd_face_roll_t {face, value}
qd_die_face_value(&die, f)    qd_die_face_label(&die, f)    qd_die_face_ppm(&die, f)
qd_roll_percentile(bonus, &pct) → 1..100     /* CoC bonus (+) / penalty (-) dice */

qd_deck_poker(&deck, jokers)  qd_deck_tarot(&deck)   qd_deck_custom(&deck, cards, n)
qd_deck_deal(&deck, out, n)   → dealt        qd_deck_draw(&deck, &one) → bool
qd_deck_shuffle(&deck)        qd_deck_return(&deck, index)
qd_deck_remaining(&deck)      qd_deck_out_count(&deck)    qd_deck_is_out(&deck, index)
qd_card_order(card)           qd_card_name(card, buf, size)
deck.policy = QD_RESHUFFLE_EVERY_DRAW | _WHEN_EMPTY | _AFTER_TRIGGER
```

## Building rolls ([more](reference/rolls.md))

```c
qd_roll_init(r)
qd_roll_dice(r, count, sides)        → p      /* + NdS */
qd_roll_dice_minus(r, count, sides)  → p      /* - NdS */
qd_roll_custom(r, count, &die)       → p
qd_roll_percent(r, count, bonus)     → p      /* d% */
qd_roll_digits(r, count, "66")       → p      /* dd66 */
qd_roll_add(r, n)   qd_roll_mul(r, n)   qd_roll_div(r, n)
qd_roll_bucket(r, t)                 → bool
qd_roll_vs(r, target)                         /* "vs N" */

qd_pool_advantage(p, n)                       /* + adv, - dis */
qd_pool_explode(p, t, forever)       → bool   /* e / ie */
qd_pool_implode(p, t)                → bool   /* i */
qd_pool_reroll(p, t, forever)        → bool   /* r / ir */
qd_pool_keep(p, QD_KEEP_HIGH | _LOW | _MIDDLE | QD_DROP_LOW | _HIGH, n)
qd_pool_success(p, t)  qd_pool_twice(p, t)  qd_pool_failure(p, t)  qd_pool_botch(p, t)
p->sets = true;  p->name;  p->hidden;

qd_test_ge(a) _le _gt _lt _eq _ne(a)  qd_test_range(a, b)  qd_test_max()  qd_test_min()  qd_test_label("+")
```

## Running ([more](reference/rolls.md#qd_roll_run))

```c
qd_roll_run(r, res)                  → err
qd_roll_check(r)                     → err
qd_err_text(err)                     → "explodes or rerolls on every face ..."
qd_roll_warnings(r)  qd_warn_text(w)
qd_reroll_all(r, res)                → err    /* benny, push */
qd_reroll_dice(r, res, which, n)     → err    /* chosen dice */
qd_reroll_matching(r, res, pool, t)  → count  /* dice passing a test */
qd_reroll_keep(r, res, higher)       → bool   /* better / worse of two */
qd_roll_observe(fn, ctx, &old_ctx)            /* see every die */
```

`res->total`, `res->successes` (if `res->flags & QD_RES_COUNTING`), `res->dice[0 .. dice_count - 1]` (`.value`, `.flags & QD_DIE_DROPPED ...`), `res->pools[k]`, `res->buckets[]`.

## Game systems ([more](reference/systems.md))

```c
qd_sys_dnd(r, QD_DND_ATTACK | _CHECK | _SAVE, adv)
qd_sys_sw(r, trait_sides, wild_card)
qd_sys_coc(r, skill, bonus)          qd_roll_vs(r, QD_COC_HARD)
qd_sys_fate(r)   qd_sys_vtm(r, pool, hunger)   qd_sys_dcc(r, count, sides, steps)
qd_sys_outcome(r, res, &out)         → bool   /* out.text "Success, 1 raise", out.level, out.pass */
qd_sys_level_name(r, level)   qd_sys_name(id)   qd_fate_ladder(v)   qd_dcc_chain(sides, steps)
```

## Notation ([guide](05-notation.md), [C](reference/parse.md))

```c
qd_parse(text, &parsed, &err)        → bool   /* err.pos, err.len, err.message */
qd_parse_line(text, rolls, max, &err) → n     /* "2d6 ; 1d20" */
qd_set_run(&parsed, &set, each, ctx) → err    /* 6 x 4d6k3, (7 x 4d6k3) k6 */
qd_mass_run(&parsed, &mass)          → err    /* 400 x atk vs 12 */
qd_format(&parsed, buf, size)   qd_format_pool(&parsed, k, buf, size)
qd_parse_set_lookup(fn, ctx)                  /* d{name} */
```

`4d6k3+2` · `d20 adv` · `3d6 e` · `1d6 ie` · `1d10 i1 e10` · `2d6 r1` · `4d6 d1` · `6d10 t8 f1` · `5d10 t7 ds10` · `100d6 [<3, =6]` · `d%` `d% b2` · `4dF` · `dd66` · `2d{Loaded}` · `(Bite) 1d20+5 ! note` · `6 x 4d6k3` · `(7 x 4d6k3) k6` · `1d20+5 vs 15` · `atk adv + 5 vs 15` · `sw8 + 1 vs 6` · `coc 65 b1 vs hard` · `fate + 2 vs 3` · `vtm 7 h2 vs 3` · `dcc d20 +1d`

## Odds and stats ([odds](reference/odds.md), [stats](reference/stats.md))

```c
qd_odds(r, &odds)                    → err    /* odds.pass, .level[L + 4], .mean, .dist[], .exact */
qd_odds_at_least(&odds, v)   qd_odds_exactly(&odds, v)
qd_stats_clear(&s)   qd_stats_watch(&s)   qd_stats_add(&s, sides, die, face, value)
qd_stats_find(&s, sides, die)   qd_stats_report(kind, &rep)   qd_chi_limit(df)
```

## Dice in text ([more](reference/text.md))

```c
qd_roll_text("2d6+1", &v)            → bool   qd_roll_textn(text, n, &v)
qd_range_find(ranges, n, v)          → index  /* no parser needed */
qd_pick_range("2d6", ranges, n, &v)  → index
qd_expand("{1d4+1} wolves", out, size, &info)
qd_expand_ex(text, out, size, &info, hook, ctx)   /* [brackets]: your hook */
```

## Fair mode ([guide](07-fair-mode.md), [C](reference/fair.md))

```c
qd_fair_start()   qd_fair_fingerprint_words(buf, size)   qd_fair_begin() → n   qd_fair_end(key)
qd_fair_start_key(key)   qd_fair_begin_at(n)   qd_fair_is_on()   qd_fair_count()
qd_fair_words(bytes, n, buf, size)   qd_fair_read_words(text, out, max)   qd_sha256(data, len, out)
```

## Sizes

Flash: generator 2 KB, decks 5.5 KB, roll engine 12 KB, with game outcomes 18 KB, with the parser 39 KB. RAM: 16 KB for the engine's working space at 1,000 dice per roll, 1.6 KB at 100. [What QD4P costs](../SIZES.md).
