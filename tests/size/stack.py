#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
stack.py - the most stack each QD4P entry point can use on the Pico 2.

    python3 tests/size/stack.py         (needs arm-none-eabi-gcc on the PATH)

WHY IT MATTERS
    The Pico SDK reserves 2 KB of stack for main() (PICO_STACK_SIZE). Go
    past it and nothing stops you: the stack quietly grows into whatever
    memory is below it. On the RP2350 that is, by default, the rest of a
    4 KB memory bank and then core 1's stack, so a small overrun goes
    unnoticed right up until the day it doesn't.

HOW IT WORKS
    GCC compiles the library for the Cortex-M33, as the Pico SDK does, with
    -fcallgraph-info=su: for every function, how much stack its own frame
    takes and which functions it calls. The worst case for an entry point
    is its frame plus its hungriest callee's worst case, all the way down.

    Recursion is where a call graph can't count by itself: the parser calls
    itself once per level of brackets, and the example table system once
    per level of [links]. Those loops are counted as many times as the
    limits allow: QD_MAX_NESTING levels of brackets, TBL_DEPTH of links.
"""
import glob
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
LIB = os.path.join(ROOT, "qd4p")
TABLES = os.path.join(ROOT, "examples", "tables")
CFLAGS = ["-mcpu=cortex-m33", "-mthumb", "-O2", "-std=c11", "-ffunction-sections",
          "-fdata-sections", "-I" + LIB, "-I" + TABLES]

ENTRIES = [
    ("qd_roll_run", "any roll"),
    ("qd_reroll_keep", "better-of-two reroll"),
    ("qd_sys_outcome", "judging a result"),
    ("qd_deck_deal", "dealing cards"),
    ("qd_parse", "reading notation"),
    ("qd_set_run", "a repeat or set: 6 x 4d6k3"),
    ("qd_mass_run", "a mass roll: 400 x atk vs 12"),
    ("qd_odds", "odds"),
    ("qd_stats_report", "the fairness check"),
    ("qd_expand", "dice inside text"),
    ("qd_fair_start", "starting fair mode"),
    ("qd_fair_begin", "a fair-mode action"),
    ("tbl_roll", "the example table system"),
]


def config(name, path):
    m = re.search(r"#define\s+%s\s+(\d+)" % name, open(path).read())
    return int(m.group(1))


def main():
    out = tempfile.mkdtemp()
    sources = [f for f in glob.glob(os.path.join(LIB, "qd_*.c")) if not f.endswith(("_pico.c", "_host.c"))]
    sources.append(os.path.join(TABLES, "table.c"))
    for src in sources:
        base = os.path.join(out, os.path.basename(src)[:-2])
        r = subprocess.run(["arm-none-eabi-gcc", *CFLAGS, "-fcallgraph-info=su", "-c", src,
                            "-o", base + ".o", "-dumpbase", base], capture_output=True, text=True)
        if r.returncode:
            sys.exit(r.stderr)

    frame, calls = {}, {}
    for ci in glob.glob(os.path.join(out, "*.ci")):
        text = open(ci).read()
        for m in re.finditer(r'node: \{ title: "([^"]+)" label: "([^"]*)"', text):
            name = m.group(1).split(":")[-1]
            su = re.search(r"(\d+) bytes", m.group(2))
            frame[name] = max(frame.get(name, 0), int(su.group(1)) if su else 0)
        for m in re.finditer(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"', text):
            calls.setdefault(m.group(1).split(":")[-1], set()).add(m.group(2).split(":")[-1])

    def worst(f, path=()):
        """(bytes, chain) for the deepest chain from f, recursion counted once."""
        if f in path:
            return 0, []
        best, chain = 0, []
        for g in calls.get(f, ()):
            b, c = worst(g, path + (f,))
            if b > best:
                best, chain = b, c
        return frame.get(f, 0) + best, [f] + chain

    nesting = config("QD_MAX_NESTING", os.path.join(LIB, "qd_config.h"))
    depth = config("TBL_DEPTH", os.path.join(TABLES, "table.h"))
    bracket = sum(frame.get(f, 0) for f in ("unary", "sum_rest", "product_rest"))
    link = sum(frame.get(f, 0) for f in ("roll_into", "roll_into.part.0", "expand_into",
                                         "qd_expand_ex", "link_hook"))
    link -= min(frame.get("roll_into", 0), frame.get("roll_into.part.0", 0))  # one of the two

    print("Stack, worst case, on the Pico 2 (arm-none-eabi-gcc %s, -O2):"
          % subprocess.run(["arm-none-eabi-gcc", "-dumpversion"], capture_output=True, text=True).stdout.strip())
    print()
    print("| Entry point | For | Worst case | Deepest chain |")
    print("|---|---|---:|---|")
    for f, what in ENTRIES:
        b, chain = worst(f)
        extra = ""
        if "unary" in chain or f in ("qd_parse", "qd_expand", "tbl_roll"):
            b += bracket * (nesting - 1)
            extra = " + %d levels of brackets" % (nesting - 1)
        if f == "tbl_roll":
            b += link * (depth - 1)
            extra += " + %d levels of links" % (depth - 1)
        print("| `%s` | %s | %d bytes | %s%s |" % (f, what, b, " > ".join(chain[:6]) + (" ..." if len(chain) > 6 else ""), extra))
    print()
    print("Each level of brackets costs %d bytes (QD_MAX_NESTING = %d); each level of table links %d"
          " (TBL_DEPTH = %d)." % (bracket, nesting, link, depth))


if __name__ == "__main__":
    main()
