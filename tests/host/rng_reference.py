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

    def d66(self):
        t = self.range(6) + 1
        u = self.range(6) + 1
        return 10 * t + u

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


POKER = [r + s for s in "CDHS" for r in
         ["2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"]] + ["JkR", "JkB", "Jk3", "Jk4"]
LOADED = [(1, 1), (2, 1), (3, 1), (4, 1), (5, 1), (6, 3)]
FUDGE = [(-1, 1), (-1, 1), (0, 1), (0, 1), (1, 1), (1, 1)]


if __name__ == "__main__":
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
    print("d66:", " ".join(str(r.d66()) for _ in range(10)))
    r = Ref(seed)
    print("pct b2:", "; ".join("%d %s %d %d" % (u, t, k, v) for u, t, k, v in (r.percentile(2) for _ in range(4))))
    r = Ref(seed)
    print("pct p1:", "; ".join("%d %s %d %d" % (u, t, k, v) for u, t, k, v in (r.percentile(-1) for _ in range(4))))
    r = Ref(seed)
    print("poker+2 deal 8:", " ".join(POKER[i] for i in r.deal(set(), 54, 8)))
