#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
notation_examples.py - every roll the notation guide shows must parse, and
every example in its table of errors must fail with that error.

    python3 tests/host/notation_examples.py

What it takes from docs/manual/05-notation.md:
  - each line of a plain code block (no language): the roll is the text
    before the first run of two or more spaces ("3d6        three d6")
  - the Example column of the quick reference and the Roll column of the
    recipes: every `backticked` roll
  - the table of errors: each `backticked` example must fail, with a
    message containing the start of that row's message
Skipped: the console transcript under Errors, the Grammar block, and C.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
GUIDE = os.path.join(ROOT, "docs", "manual", "05-notation.md")
BUILD = os.path.join(HERE, "build")
LIB = os.path.join(ROOT, "qd4p")
CC = os.environ.get("CC", "cc")


def cells(row):
    return [c.strip() for c in row.strip().strip("|").split("|")]


def ticked(text):
    return re.findall(r"`([^`]+)`", text)


def main():
    text = open(GUIDE, encoding="utf-8").read()
    lines = []
    section = ""
    in_block = False
    lang = ""
    header = None
    for raw in text.split("\n"):
        if raw.startswith("## "):
            section = raw[3:].strip()
            header = None
        if raw.startswith("```"):
            if not in_block:
                in_block, lang = True, raw[3:].strip()
            else:
                in_block = False
            continue
        if in_block:
            if lang or section in ("Grammar", "Errors"):
                continue
            roll = re.split(r"\s{2,}", raw.strip())[0]
            if roll and not roll.startswith((">", "#")):
                lines.append("OK\t" + roll)
            continue
        if raw.startswith("|") and not raw.startswith("| ---") and not raw.startswith("|---"):
            row = cells(raw)
            if header is None:
                header = row
                continue
            if section == "Quick reference" and "Example" in header:
                lines += ["OK\t" + t for t in ticked(row[header.index("Example")])]
            elif section == "Recipes for popular games" and "Roll" in header:
                lines += ["OK\t" + t for t in ticked(row[header.index("Roll")])
                          if not t.startswith("1d20 adv,")]
            elif section == "Errors" and len(row) >= 2:
                message = row[0].replace("…", "...")
                # each message up to "..." or a placeholder N ("to roll it N times")
                words = "|".join(re.split(r"\.\.\.| N\b", w.strip())[0].strip().rstrip("'\"").strip()
                                 for w in message.split(" / "))
                lines += ["ERR\t%s\t%s" % (words, t) for t in ticked(row[1])]
        else:
            header = None if not raw.startswith("|") else header

    os.makedirs(BUILD, exist_ok=True)
    exe = os.path.join(BUILD, "notation_check")
    sources = [os.path.join(LIB, f) for f in sorted(os.listdir(LIB))
               if f.endswith(".c") and not f.endswith("_pico.c")]
    r = subprocess.run([CC, "-std=c11", "-O1", "-I" + LIB, "-o", exe,
                        os.path.join(HERE, "notation_check.c"), *sources, "-lm"],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stderr)
    r = subprocess.run([exe], input="\n".join(lines) + "\n", capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    if r.returncode:
        print("notation guide: %d examples don't do what the guide says" % r.returncode)
    sys.exit(1 if r.returncode else 0)


if __name__ == "__main__":
    main()
