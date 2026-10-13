/* SPDX-License-Identifier: MIT-0 */
/* SPDX-AI-Disclosure: ai-generated */
/* SPDX-AI-Model: claude-opus-5-5 */
/* SPDX-AI-Provider: Anthropic */
/**
 * @file    qd_sys.c
 * @brief   Game systems: building their rolls and judging the results.
 *
 * Each system is two small pieces: a builder that puts the system's dice
 * at the start of a roll (pools and a few sum steps), and an outcome rule
 * that reads the result. The rules are written out plainly, one function
 * per system, with the rulebook's wording in the comments.
 */
#include <stdio.h>
#include <string.h>

#include "qd_sys.h"

/* ========================================================================
 * Building
 * ======================================================================== */
void qd_roll_vs(qd_roll_t *r, int32_t target)
{
    r->sys.has_target = true;
    r->sys.target = target;
}

/* Every builder starts from an empty roll and records how many sum steps
 * it made, so qd_format can write the shortcut back instead of them. */
static bool begin(qd_roll_t *r, uint8_t id)
{
    if (r->op_count || r->pool_count) {
        r->error = QD_ERR_SUM;
        return false;
    }
    memset(&r->sys, 0, sizeof(r->sys));
    r->sys.id = id;
    return true;
}

static bool finish(qd_roll_t *r)
{
    r->sys.ops = r->op_count;
    return !r->error;
}

bool qd_sys_dnd(qd_roll_t *r, int kind, int adv)
{
    if (!begin(r, QD_SYS_DND))
        return false;
    r->sys.kind = (uint8_t)kind;
    r->sys.a = (int16_t)(adv > 0 ? 1 : adv < 0 ? -1 : 0);
    qd_pool_t *p = qd_roll_dice(r, 1, 20);
    if (p) qd_pool_advantage(p, r->sys.a);
    return finish(r);
}

bool qd_sys_sw(qd_roll_t *r, int trait_sides, bool wild_card)
{
    if (trait_sides != 4 && trait_sides != 6 && trait_sides != 8 && trait_sides != 10 &&
        trait_sides != 12)
        return false;
    if (!begin(r, QD_SYS_SW))
        return false;
    r->sys.a = (int16_t)trait_sides;
    r->sys.b = wild_card;
    /* "Trait and Wild Dice both Ace": each explodes on its highest face. */
    qd_pool_t *t = qd_roll_new_pool(r);
    if (!t) return finish(r);
    t->count = 1;
    t->sides = (uint16_t)trait_sides;
    strcpy(t->name, "trait");
    qd_pool_explode(t, qd_test_max(), true);
    qd_roll_push(r, QD_OP_POOL, 0);
    if (wild_card) {
        qd_pool_t *w = qd_roll_new_pool(r);
        if (!w) return finish(r);
        w->count = 1;
        w->sides = 6;
        strcpy(w->name, "wild");
        qd_pool_explode(w, qd_test_max(), true);
        qd_roll_push(r, QD_OP_POOL, 1);
        qd_roll_push(r, QD_OP_MAX, 0);          /* "use the higher of the two" */
    }
    return finish(r);
}

bool qd_sys_coc(qd_roll_t *r, int skill, int bonus)
{
    if (skill < 0 || skill > 999 || bonus > QD_MAX_BONUS_DICE || bonus < -QD_MAX_BONUS_DICE)
        return false;
    if (!begin(r, QD_SYS_COC))
        return false;
    r->sys.a = (int16_t)skill;
    r->sys.b = (int16_t)bonus;
    qd_roll_percent(r, 1, bonus);
    return finish(r);
}

bool qd_sys_fate(qd_roll_t *r)
{
    if (!begin(r, QD_SYS_FATE))
        return false;
    qd_roll_custom(r, 4, qd_fudge_die());
    return finish(r);
}

bool qd_sys_vtm(qd_roll_t *r, int pool, int hunger)
{
    if (pool < 1 || pool > 50 || hunger < 0 || hunger > 5)
        return false;
    if (!begin(r, QD_SYS_VTM))
        return false;
    if (hunger > pool) hunger = pool;          /* "replace that many dice": never more */
    r->sys.a = (int16_t)pool;
    r->sys.b = (int16_t)hunger;
    /* Every die is a success on 6+. Regular dice first, then hunger dice. */
    if (pool > hunger) {
        qd_pool_t *p = qd_roll_dice(r, (uint16_t)(pool - hunger), 10);
        if (p) qd_pool_success(p, qd_test_ge(6));
    }
    if (hunger) {
        qd_pool_t *h = qd_roll_dice(r, (uint16_t)hunger, 10);
        if (h) {
            qd_pool_success(h, qd_test_ge(6));
            strcpy(h->name, "hunger");
        }
    }
    return finish(r);
}

static const int16_t chain[] = { 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 30 };
#define CHAIN_LEN ((int)(sizeof(chain) / sizeof(chain[0])))

int qd_dcc_chain(int sides, int steps)
{
    for (int i = 0; i < CHAIN_LEN; i++)
        if (chain[i] == sides) {
            int j = i + steps;
            if (j < 0) j = 0;
            if (j >= CHAIN_LEN) j = CHAIN_LEN - 1;
            return chain[j];
        }
    return 0;
}

bool qd_sys_dcc(qd_roll_t *r, int count, int sides, int steps)
{
    int moved = qd_dcc_chain(sides, steps);
    if (!moved || count < 1 || count > QD_MAX_ROLL_DICE)
        return false;
    if (!begin(r, QD_SYS_DCC))
        return false;
    r->sys.a = (int16_t)moved;
    r->sys.b = (int16_t)steps;
    r->sys.c = (int16_t)sides;
    r->sys.kind = (uint8_t)count;
    qd_roll_dice(r, (uint16_t)count, (uint16_t)moved);
    return finish(r);
}

/* ========================================================================
 * Judging
 * ======================================================================== */
const char *qd_sys_name(int id)
{
    switch (id) {
    case QD_SYS_DND:  return "D&D 5e";
    case QD_SYS_SW:   return "Savage Worlds";
    case QD_SYS_COC:  return "Call of Cthulhu 7e";
    case QD_SYS_FATE: return "FATE";
    case QD_SYS_VTM:  return "Vampire 5e";
    case QD_SYS_DCC:  return "DCC";
    default:          return "";
    }
}

const char *qd_fate_ladder(int v)
{
    static const char *const names[] = { "Terrible", "Poor", "Mediocre", "Average", "Fair",
        "Good", "Great", "Superb", "Fantastic", "Epic", "Legendary" };
    if (v < -2) v = -2;
    if (v > 8) v = 8;
    return names[v + 2];
}

/* The first die still in play in a pool: the d20 that counted, after
 * advantage set the other aside. 0 if there isn't one listed. */
static int16_t natural(const qd_result_t *res, int pool)
{
    const qd_pool_result_t *pr = &res->pools[pool];
    for (int i = 0; i < pr->listed; i++) {
        const qd_die_result_t *d = &res->dice[pr->first + i];
        if (!(d->flags & QD_DIE_OUT))
            return d->value;
    }
    return 0;
}

/* The die a pool rolled first, before any explosion: the "natural" roll. */
static int16_t first_die(const qd_result_t *res, int pool)
{
    const qd_pool_result_t *pr = &res->pools[pool];
    return pr->listed ? res->dice[pr->first].value : 0;
}

static int count_flags(const qd_result_t *res, int pool, uint16_t flag)
{
    const qd_pool_result_t *pr = &res->pools[pool];
    int n = 0;
    for (int i = 0; i < pr->listed; i++)
        n += !!(res->dice[pr->first + i].flags & flag);
    return n;
}

static int count_value(const qd_result_t *res, int pool, int16_t v)
{
    const qd_pool_result_t *pr = &res->pools[pool];
    int n = 0;
    for (int i = 0; i < pr->listed; i++) {
        const qd_die_result_t *d = &res->dice[pr->first + i];
        n += !(d->flags & QD_DIE_OUT) && d->value == v;
    }
    return n;
}

#define TEXT(...)   snprintf(out->text, sizeof(out->text), __VA_ARGS__)
#define DETAIL(...) snprintf(out->detail, sizeof(out->detail), __VA_ARGS__)

static void judge(qd_outcome_t *out, int32_t value, int32_t target)
{
    out->judged = true;
    out->value = value;
    out->target = target;
    out->margin = value - target;
    out->pass = value >= target;
}

/* Plain notation with "vs N": the total, or the successes if the roll
 * counts them, must reach N. */
static void plain(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out)
{
    int32_t v = (res->flags & QD_RES_COUNTING) ? res->successes : res->total;
    judge(out, v, r->sys.target);
    out->level = out->pass ? QD_LVL_PASS : QD_LVL_FAIL;
    TEXT("%s", out->pass ? "Pass" : "Fail");
    if (out->margin > 0) DETAIL("by %ld", (long)out->margin);
    else if (out->margin < 0) DETAIL("short by %ld", (long)-out->margin);
    else DETAIL("exactly");
}

/* D&D 5e. "If the d20 roll for an attack is a 20, the attack hits
 * regardless of any modifiers or the target's AC ... a critical hit. If the
 * d20 roll for an attack is a 1, the attack misses regardless." Checks and
 * saves have no such rule, so a natural 20 or 1 is only noted. */
static void dnd(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out)
{
    int16_t nat = natural(res, 0);
    bool attack = r->sys.kind == QD_DND_ATTACK;
    out->value = res->total;
    out->critical = nat == 20;
    out->fumble = nat == 1;
    if (nat == 20 || nat == 1) DETAIL("natural %d", nat);
    if (r->sys.has_target) judge(out, res->total, r->sys.target);
    if (attack && nat == 20) {
        out->pass = true;
        out->level = QD_LVL_CRIT;
        TEXT("Critical hit");
    } else if (attack && nat == 1) {
        out->pass = false;
        out->level = QD_LVL_FAIL;
        TEXT("Miss");
    } else if (out->judged) {
        out->level = out->pass ? QD_LVL_PASS : QD_LVL_FAIL;
        TEXT("%s", attack ? (out->pass ? "Hit" : "Miss") : (out->pass ? "Success" : "Failure"));
    } else if (nat == 20 || nat == 1) {
        out->level = QD_LVL_NONE;
        TEXT("Natural %d", nat);
        out->detail[0] = '\0';
    }
}

/* Savage Worlds (SWADE). Target Number 4 unless told otherwise. "Every 4
 * points over the Target Number is a raise." "If a Wild Card rolls a 1 on
 * both the trait and Wild Die, it's a Critical Failure" - whatever the
 * total. Each die that exploded is an ace. */
static void savage(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out)
{
    int32_t tn = r->sys.has_target ? r->sys.target : 4;
    bool wild = r->sys.b;
    judge(out, res->total, tn);
    int aces = count_flags(res, 0, QD_DIE_EXPLODED) + (wild ? count_flags(res, 1, QD_DIE_EXPLODED) : 0);
    out->fumble = wild && first_die(res, 0) == 1 && first_die(res, 1) == 1;
    if (out->fumble) {
        out->pass = false;
        out->level = QD_LVL_CRIT_FAIL;
        TEXT("Critical failure");
        DETAIL("snake eyes");
        return;
    }
    if (!out->pass) {
        out->level = QD_LVL_FAIL;
        TEXT("Failure");
    } else {
        out->degrees = (int16_t)(out->margin / 4);
        out->level = out->degrees ? QD_LVL_RAISE : QD_LVL_PASS;
        if (out->degrees)
            TEXT("Success, %d raise%s", out->degrees, out->degrees == 1 ? "" : "s");
        else
            TEXT("Success");
    }
    if (aces)
        DETAIL("%d ace%s, vs %ld", aces, aces == 1 ? "" : "s", (long)tn);
    else
        DETAIL("vs %ld", (long)tn);
}

/* Call of Cthulhu 7e. Rolled under the skill: Regular (<= skill), Hard
 * (<= half), Extreme (<= a fifth), Critical (01). Fumble: 100, or 96-100
 * when the skill is under 50. A difficulty (Regular, Hard, Extreme) must
 * be met or beaten; a critical always succeeds and a fumble always fails. */
static void coc(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out)
{
    static const char *const names[] = { "Regular success", "Hard success", "Extreme success",
                                         "Critical success" };
    static const char *const needs[] = { "", "Regular", "Hard", "Extreme" };
    int skill = r->sys.a;
    int v = res->pools[0].total;
    int need = r->sys.has_target ? (int)r->sys.target : QD_COC_REGULAR;
    if (need < QD_COC_REGULAR) need = QD_COC_REGULAR;
    if (need > QD_COC_EXTREME) need = QD_COC_EXTREME;
    out->value = v;
    out->target = skill;
    out->judged = true;
    out->margin = skill - v;                      /* how far under the skill */

    if (v == 1)                                    out->level = QD_COC_CRITICAL;
    else if (v == 100 || (skill < 50 && v >= 96))  out->level = QD_COC_FUMBLE;
    else if (v <= skill / 5)                       out->level = QD_COC_EXTREME_SUCCESS;
    else if (v <= skill / 2)                       out->level = QD_COC_HARD_SUCCESS;
    else if (v <= skill)                           out->level = QD_COC_REGULAR_SUCCESS;
    else                                           out->level = QD_COC_FAILURE;

    out->critical = out->level == QD_COC_CRITICAL;
    out->fumble = out->level == QD_COC_FUMBLE;
    out->pass = out->critical || (out->level > 0 && out->level >= need);
    if (out->level == QD_COC_FUMBLE)
        TEXT("Fumble");
    else if (out->level < 0)
        TEXT("Failure");
    else if (out->pass)
        TEXT("%s", names[out->level - 1]);
    else
        TEXT("Failure (%s, %s needed)", names[out->level - 1], needs[need]);
    DETAIL("skill %d: hard %d, extreme %d", skill, skill / 2, skill / 5);
}

/* FATE. The total is read on the ladder. Against opposition, the
 * difference is shifts: below 0 fails, 0 ties, 1-2 succeeds, 3+ succeeds
 * with style. */
static void fate(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out)
{
    int32_t v = res->total;
    out->value = v;
    if (!r->sys.has_target) {
        TEXT("%s (%+ld)", qd_fate_ladder((int)v), (long)v);
        return;
    }
    judge(out, v, r->sys.target);
    out->degrees = (int16_t)out->margin;
    if (out->margin < 0)       { out->level = QD_FATE_FAIL;    TEXT("Fail"); }
    else if (out->margin == 0) { out->level = QD_FATE_TIE;     TEXT("Tie"); }
    else if (out->margin < 3)  { out->level = QD_FATE_SUCCESS; TEXT("Success"); }
    else                       { out->level = QD_FATE_STYLE;   TEXT("Success with style"); }
    DETAIL("%s (%+ld), %ld shift%s", qd_fate_ladder((int)v), (long)v, (long)out->margin,
           out->margin == 1 || out->margin == -1 ? "" : "s");
}

/* Vampire 5e. Each 6+ is a success; each pair of 10s is a critical,
 * worth 4 successes instead of 2. A critical win with a 10 on a hunger
 * die is a messy critical. A failure with a 1 on a hunger die is a bestial
 * failure. No successes at all is a total failure. */
static void vtm(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out)
{
    int hunger = r->sys.b, regular = r->sys.a - hunger;
    int hp = regular > 0 ? 1 : 0;                       /* the hunger pool's index */
    int tens = 0, hunger_tens = 0, hunger_ones = 0, plain = 0;
    for (int k = 0; k < res->pool_count; k++) {
        tens += count_value(res, k, 10);
        plain += res->pools[k].successes;
    }
    if (hunger) {
        hunger_tens = count_value(res, hp, 10);
        hunger_ones = count_value(res, hp, 1);
    }
    int pairs = tens / 2;
    int s = plain + 2 * pairs;
    out->value = s;
    out->critical = pairs > 0;
    bool messy = out->critical && hunger_tens > 0;
    if (r->sys.has_target) {
        judge(out, s, r->sys.target);
        out->degrees = (int16_t)out->margin;
    }
    bool failed = out->judged ? !out->pass : s == 0;
    if (failed && hunger_ones) {
        out->level = QD_VTM_BESTIAL;
        out->fumble = true;
        TEXT("Bestial failure");
    } else if (s == 0) {
        out->level = QD_VTM_TOTAL_FAIL;
        TEXT("Total failure");
    } else if (failed) {
        out->level = QD_VTM_FAIL;
        TEXT("Failure");
    } else if (messy) {
        out->level = QD_VTM_MESSY;
        TEXT("Messy critical");
    } else if (out->critical) {
        out->level = QD_VTM_CRITICAL;
        TEXT("Critical");
    } else {
        out->level = out->judged ? QD_VTM_WIN : QD_LVL_NONE;
        if (out->judged) TEXT("Win");
        else TEXT("%d success%s", s, s == 1 ? "" : "es");
    }
    if (out->judged && pairs)
        DETAIL("%d successes, %d from %s of 10s, vs %ld", s, 4 * pairs,
               pairs == 1 ? "a pair" : "pairs", (long)r->sys.target);
    else if (out->judged)
        DETAIL("%d success%s vs %ld", s, s == 1 ? "" : "es", (long)r->sys.target);
    else if (hunger_ones && s > 0)
        DETAIL("hunger 1s: bestial if it fails");
    else if (out->critical)
        DETAIL("%d successes, %d from %s of 10s", s, 4 * pairs, pairs == 1 ? "a pair" : "pairs");
}

/* Dungeon Crawl Classics. A natural roll of the die's highest face is a
 * critical and a natural 1 a fumble (the judge rules on the details). */
static void dcc(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out)
{
    int sides = r->sys.a;
    int16_t nat = natural(res, 0);
    out->value = res->total;
    out->critical = nat == sides;
    out->fumble = nat == 1;
    if (r->sys.has_target) judge(out, res->total, r->sys.target);
    if (out->critical) {
        out->level = QD_LVL_CRIT;
        out->pass = true;
        TEXT("Critical");
    } else if (out->fumble) {
        out->level = QD_LVL_CRIT_FAIL;
        out->pass = false;
        TEXT("Fumble");
    } else if (out->judged) {
        out->level = out->pass ? QD_LVL_PASS : QD_LVL_FAIL;
        TEXT("%s", out->pass ? "Success" : "Failure");
    }
    if (r->sys.b)
        DETAIL("natural %d on d%d (d%d %+dd)", nat, sides, r->sys.c, r->sys.b);
    else
        DETAIL("natural %d on d%d", nat, sides);
}

bool qd_sys_outcome(const qd_roll_t *r, const qd_result_t *res, qd_outcome_t *out)
{
    memset(out, 0, sizeof(*out));
    out->system = r->sys.id;
    out->value = res->total;
    switch (r->sys.id) {
    case QD_SYS_DND:  dnd(r, res, out);    return true;
    case QD_SYS_SW:   savage(r, res, out); return true;
    case QD_SYS_COC:  coc(r, res, out);    return true;
    case QD_SYS_FATE: fate(r, res, out);   return true;
    case QD_SYS_VTM:  vtm(r, res, out);    return true;
    case QD_SYS_DCC:  dcc(r, res, out);    return true;
    default:
        if (!r->sys.has_target)
            return false;
        plain(r, res, out);
        return true;
    }
}
