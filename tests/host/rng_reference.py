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


if __name__ == "__main__":
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 42
    r = Ref(seed)
    print("next32:", " ".join(str(r.next32()) for _ in range(8)))
    r = Ref(seed)
    print("d6:", " ".join(str(r.range(6) + 1) for _ in range(20)))
