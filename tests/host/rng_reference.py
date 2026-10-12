#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
rng_reference.py - an independent Python version of QD4P's generator, used
to check the C code: same seed, same numbers, or something is wrong.

    python3 rng_reference.py 42        # first raw outputs and d6 rolls for seed 42

Written from the published algorithms (splitmix64, xoshiro128**, Lemire's
unbiased range), not from the C file, so the two check each other.
"""
import sys

M32, M64 = 0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF


class Ref:
    def __init__(self, seed):
        x = seed & M64
        words = []
        for _ in range(2):
            x = (x + 0x9E3779B97F4A7C15) & M64
            z = x
            z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
            z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
            z ^= z >> 31
            words += [z & M32, z >> 32]
        self.s = words
        if not any(self.s):
            self.s[0] = 0x9E3779B9

    @staticmethod
    def rotl(v, k):
        return ((v << k) | (v >> (32 - k))) & M32

    def next32(self):
        s = self.s
        result = (self.rotl((s[1] * 5) & M32, 7) * 9) & M32
        t = (s[1] << 9) & M32
        s[2] ^= s[0]
        s[3] ^= s[1]
        s[1] ^= s[2]
        s[0] ^= s[3]
        s[2] ^= t
        s[3] = self.rotl(s[3], 11)
        return result

    def range(self, n):
        m = self.next32() * n
        low = m & M32
        if low < n:
            threshold = (2**32 - n) % n
            while low < threshold:
                m = self.next32() * n
                low = m & M32
        return m >> 32


    # --- D2: dice and decks, in the same order of draws as qd_dice.c / qd_deck.c ---
    def weighted(self, faces):
        """faces: list of (value, weight). Returns (face index, value)."""
        if all(w == 1 for _, w in faces):
            i = self.range(len(faces))
        else:
            ticket = self.range(sum(w for _, w in faces))
            i = 0
            while ticket >= faces[i][1]:
                ticket -= faces[i][1]
                i += 1
        return i, faces[i][0]

    def digits(self, sides):
        """Digit dice: one die per digit, left to right. dd66 -> sides [6, 6]."""
        v = 0
        for s in sides:
            v = v * 10 + self.range(s) + 1
        return v

    def percentile(self, bonus):
        bonus = max(-3, min(3, bonus))
        units = self.range(10)
        tens = []
        kept = 0
        val = lambda t: (t * 10 + units) or 100
        for i in range(1 + abs(bonus)):
            tens.append(self.range(10))
            if (bonus > 0 and val(tens[i]) < val(tens[kept])) or \
               (bonus < 0 and val(tens[i]) > val(tens[kept])):
                kept = i
        return units, tens, kept, val(tens[kept])

    def deal(self, out_set, count, n):
        """Deal n cards from a deck of `count`; out_set holds cards already out."""
        got = []
        for _ in range(n):
            left = [i for i in range(count) if i not in out_set]
            if not left:
                break
            i = left[self.range(len(left))]
            out_set.add(i)
            got.append(i)
        return got

    # --- D3: the roll engine, written from the stage rules in qd_roll.h ---
    def pool(self, count, sides=None, faces=None, digits=None, adv=0, steps=(),
             keep=None, success=None, twice=None, failure=None, botch=None, cap=100):
        """Roll one pool. Returns (dice, total, successes, failures, botches)
        where dice is a list of [value, set of flags, parent]."""
        if digits:
            lo = int("1" * len(digits)); hi = int("".join(map(str, digits)))
            roll_one = lambda: (self.digits(digits), "")
        elif faces:
            vals = [f[0] for f in faces]
            lo, hi = min(vals), max(vals)
            def roll_one():
                i, v = self.weighted([(f[0], f[1]) for f in faces])
                return v, faces[i][2] if len(faces[i]) > 2 else ""
        else:
            lo, hi = 1, sides
            roll_one = lambda: (self.range(sides) + 1, "")
        dice = []

        def add(parent, flag):
            v, label = roll_one()
            dice.append([v, {flag} if flag else set(), parent, label])
            return len(dice) - 1

        def test(t, d):
            if t is None:
                return False
            op, a, b = (tuple(t) + (0, 0))[:3]
            v = d[0]
            return {"ge": v >= a, "le": v <= a, "gt": v > a, "lt": v < a, "eq": v == a,
                    "ne": v != a, "range": a <= v <= b, "max": v == hi, "min": v == lo,
                    "label": d[3] == a}[op]

        out = lambda d: d[1] & {"dropped", "aside", "rerolled", "imploded"}

        def in_play_sorted():
            idx = [i for i, d in enumerate(dice) if not out(d)]
            return sorted(idx, key=lambda i: dice[i][0])     # stable: ties keep roll order

        def mark(lo_n, hi_n, flag):
            s = in_play_sorted()
            lo_n = min(lo_n, len(s)); hi_n = min(hi_n, len(s) - lo_n)
            for i in s[:lo_n] + (s[len(s) - hi_n:] if hi_n else []):
                dice[i][1].add(flag)

        for _ in range(count + abs(adv)):
            add(None, None)
        if adv:
            mark(adv if adv > 0 else 0, -adv if adv < 0 else 0, "aside")

        for kind, forever, t in steps:
            for i in range(len(dice)):          # range() is fixed when the step starts
                d = dice[i]
                if out(d) or not test(t, d):
                    continue
                if kind == "explode":
                    last, k = i, 1
                    while True:
                        j = add(last, "exploded")
                        if not forever or not test(t, dice[j]):
                            break
                        if k >= cap:
                            break
                        last, k = j, k + 1
                elif kind == "implode":
                    add(i, "imploded")
                else:
                    cur, k = i, 1
                    while True:
                        j = add(cur, "replacement")
                        dice[cur][1].add("rerolled")
                        if not forever or not test(t, dice[j]):
                            break
                        if k >= cap:
                            break
                        cur, k = j, k + 1

        if keep:
            how, n = keep
            live = len(in_play_sorted()); n = min(n, live)
            lo_n, hi_n = {"k": (live - n, 0), "kl": (0, live - n),
                          "km": ((live - n) // 2, live - n - (live - n) // 2),
                          "d": (n, 0), "dh": (0, n)}[how]
            mark(lo_n, hi_n, "dropped")

        total = succ = fail = botches = 0
        for d in dice:
            if "imploded" in d[1]:
                total -= d[0]
                continue
            if out(d):
                continue
            total += d[0]
            if test(success, d):
                succ += 1 + test(twice, d)
            fail += test(failure, d)
            botches += test(botch, d)
        return [d[:3] for d in dice], total, succ, fail, botches


GOLDEN_POOLS = [
    ("4d6 k3",         dict(count=4, sides=6, keep=("k", 3))),
    ("10d6 e6 k8",     dict(count=10, sides=6, steps=[("explode", False, ("ge", 6))], keep=("k", 8))),
    ("6d6 ie6",        dict(count=6, sides=6, steps=[("explode", True, ("max",))])),
    ("8d10 t8 ie10 f1", dict(count=8, sides=10, success=("ge", 8), failure=("le", 1),
                            steps=[("explode", True, ("max",))])),
    ("2d20 adv1",      dict(count=2, sides=20, adv=1)),
    ("3d20 dis2",      dict(count=3, sides=20, adv=-2)),
    ("6d6 r2",         dict(count=6, sides=6, steps=[("reroll", False, ("le", 2))])),
    ("6d6 ir2",        dict(count=6, sides=6, steps=[("reroll", True, ("le", 2))])),
    ("4d10 i1 e10",    dict(count=4, sides=10, steps=[("implode", False, ("min",)),
                                                      ("explode", False, ("max",))])),
    ("5d10 km3",       dict(count=5, sides=10, keep=("km", 3))),
    ("6d10 t7 ds10",   dict(count=6, sides=10, success=("ge", 7), twice=("ge", 10))),
    ("5dd66 dh1",      dict(count=5, digits=[6, 6], keep=("dh", 1))),
    ("12d4 ie4",       dict(count=12, sides=4, steps=[("explode", True, ("max",))])),
    ("10d6 ir3 k5",    dict(count=10, sides=6, steps=[("reroll", True, ("le", 3))], keep=("k", 5))),
    ("20d6 e5 r1 i2 d4", dict(count=20, sides=6, steps=[("explode", False, ("ge", 5)),
                                                        ("reroll", False, ("eq", 1)),
                                                        ("implode", False, ("le", 2))],
                              keep=("d", 4))),
    ("8d{loaded} rg5 kl4", dict(count=8, faces=[(1, 1), (2, 1), (3, 1), (4, 1), (5, 1), (6, 3)],
                              steps=[("reroll", False, ("ge", 5))], keep=("kl", 4))),
    ("4d1000 adv3",    dict(count=4, sides=1000, adv=3)),
]

# C flag bits (QD_DIE_* in qd_roll.h) for the flags the reference tracks.
C_FLAGS = {"dropped": 1, "aside": 2, "rerolled": 4, "exploded": 8, "imploded": 16,
           "replacement": 32}


def c_goldens(seeds=range(1, 26)):
    """A C header of every golden pool for many seeds: values, flags,
    parents and totals, for test_roll.c to compare die by die."""
    out = ["/* Generated by rng_reference.py --c-goldens. Do not edit. */",
           "#define GOLDEN_SPECS %d" % len(GOLDEN_POOLS),
           "#define GOLDEN_SEEDS %d" % len(seeds),
           "typedef struct {",
           "    const char *text; int spec, seed, n, total, successes, failures;",
           "    const int16_t *values; const uint16_t *flags, *parents;",
           "} golden_t;",
           "static const golden_t goldens[] = {"]
    for k, (name, kw) in enumerate(GOLDEN_POOLS):
        for seed in seeds:
            dice, total, s, f, b = Ref(seed).pool(**kw)
            vals = ",".join(str(d[0]) for d in dice)
            flags = ",".join(str(sum(C_FLAGS[x] for x in d[1] if x in C_FLAGS)) for d in dice)
            parents = ",".join(str(0xFFFF if d[2] is None else d[2]) for d in dice)
            out.append("  { \"%s\", %d, %d, %d, %d, %d, %d, (const int16_t[]){%s}, "
                       "(const uint16_t[]){%s}, (const uint16_t[]){%s} },"
                       % (name, k, seed, len(dice), total, s, f, vals, flags, parents))
    out.append("};")
    return "\n".join(out)

POKER = [r + s for s in "CDHS" for r in
         ["2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"]] + ["JkR", "JkB", "Jk3", "Jk4"]
LOADED = [(1, 1), (2, 1), (3, 1), (4, 1), (5, 1), (6, 3)]
FUDGE = [(-1, 1), (-1, 1), (0, 1), (0, 1), (1, 1), (1, 1)]


if __name__ == "__main__":
    if sys.argv[1:] == ["--c-goldens"]:
        print(c_goldens())
        sys.exit(0)
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
    r = Ref(seed)
    print("next32:", " ".join(str(r.next32()) for _ in range(8)))
    r = Ref(seed)
    print("d6:", " ".join(str(r.range(6) + 1) for _ in range(20)))
    r = Ref(seed)
    print("loaded:", " ".join(str(r.weighted(LOADED)[1]) for _ in range(20)))
    r = Ref(seed)
    print("4dF x5:", " ".join(str(sum(r.weighted(FUDGE)[1] for _ in range(4))) for _ in range(5)))
    r = Ref(seed)
    print("dd66:", " ".join(str(r.digits([6, 6])) for _ in range(10)))
    r = Ref(seed)
    print("pct b2:", "; ".join("%d %s %d %d" % (u, t, k, v) for u, t, k, v in (r.percentile(2) for _ in range(4))))
    r = Ref(seed)
    print("pct p1:", "; ".join("%d %s %d %d" % (u, t, k, v) for u, t, k, v in (r.percentile(-1) for _ in range(4))))
    r = Ref(seed)
    print("poker+2 deal 8:", " ".join(POKER[i] for i in r.deal(set(), 54, 8)))
    for name, kw in GOLDEN_POOLS:
        r = Ref(seed)
        dice, total, s, f, b = r.pool(**kw)
        print("%s: %s -> total %d, successes %d, failures %d" %
              (name, " ".join(str(d[0]) for d in dice), total, s - f, f))
