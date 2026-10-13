#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
test_fair_pc.py - fair mode end to end, through the console built for a PC
(examples/pc): a live session, its key revealed, then every action checked
from the key, out of order, by a second run of the program.
"""
import os
import re
import subprocess
import sys

from fair_reference import fingerprint, word_list

HERE = os.path.dirname(os.path.abspath(__file__))
PROG = os.path.join(HERE, "build", "qd4p_pc")
checks = failures = 0


def check(ok, what):
    global checks, failures
    checks += 1
    if not ok:
        failures += 1
        print("FAIL:", what)


def run(lines):
    return subprocess.run([PROG], input="\n".join(lines) + "\n", capture_output=True,
                          text=True, timeout=60).stdout


def actions(out):
    """'fair #N' -> everything it printed, up to the next prompt."""
    found = {}
    for block in out.split("\n> "):
        m = re.search(r"^fair #(\d+)\n", block, re.M)
        if m:
            found.setdefault(int(m.group(1)), []).append(block[m.end():])
    return found


env = dict(os.environ, OUT=PROG)
b = subprocess.run(["sh", os.path.join(HERE, "../../examples/pc/build.sh")], env=env,
                   capture_output=True, text=True)
if b.returncode:
    print(b.stdout + b.stderr)
    sys.exit(1)

session = ["fair", "4d6k3", "atk +5 vs 15", "reroll", "stir hello", "seed 4", "table forest",
           "deck sw", "deal 5", "deal 3", "pct b1", "custom 0 1 2 3 4 5 6*3", "3d{custom}",
           "6 x 4d6k3", "1000d6", "sw8", "#2 1d20", "fair end"]
live = run(session)
check("ignored: fair mode is on" in live, "stirring is refused during a session")
check("ignored: fair mode is on ('fair end' first)" in live, "replay is refused during a session")
check("#number is for checking" in live, "#N is refused during a live session")
m = re.search(r"fingerprint - everyone write it down now:\n  ([a-z ]+)\n", live)
k = re.search(r"The key \(16 words\):\n  ([a-z ]+)\n  ([a-z ]+)\n", live)
check(m and k, "a fingerprint at the start and a key at the end")
if not (m and k):
    print(live)
    sys.exit(1)
start_print, key_words = m.group(1), k.group(1) + " " + k.group(2)
check("after 11 actions" in live, "11 actions numbered: stir and seed are not actions")

# the key gives the fingerprint, by the independent Python reference
words = word_list()
key = bytes(words.index(w) for w in key_words.split())
check(" ".join(words[b] for b in fingerprint(key)) == start_print,
      "Python agrees the key gives the fingerprint shown at the start")

# check every action, out of order; rerolls and deals after what they need
cut = " ".join(w[:3] for w in key_words.split())       # three letters each is enough
checking = ["fair check " + cut.upper(), "#11 sw8", "#10 1000d6", "#4 table forest", "#9 6 x 4d6k3",
            "#1 4d6k3", "#2 atk +5 vs 15", "#3 reroll", "deck sw", "#5 deal 5", "#6 deal 3", "#7 pct b1",
            "custom 0 1 2 3 4 5 6*3", "#8 3d{custom}", "#8 3d6", "fair", "fair end"]
out = run(checking)
check("Its fingerprint:\n  " + start_print in out, "checking shows the same fingerprint")
want, got = actions(live), actions(out)


def strip_time(text):
    return re.sub(r"\(\d+ dice in \d+ us\)", "", text)


for n in range(1, 12):
    check(n in got and strip_time(got[n][0]) == strip_time(want[n][0]),
          "action #%d checks out:\nlive:\n%s\nchecked:\n%s" % (n, want.get(n), got.get(n)))
check(got[8][1] != want[8][0], "#8 as plain 3d6 isn't the same roll as 3d{custom}")
check("checking a fair session" in out and "done checking" in out, "the checking messages")

bad = run(["fair check wood cherry", "fair check wood cherry zzz", "fair", "fair check ab"])
check("2 words so far: type the other 14" in bad and "word 3 isn't" in bad and "word 1 isn't" in bad,
      "bad key words are explained")

# the key as the device shows it: two lines of eight
halves = key_words.split()
two = run(["fair check " + " ".join(halves[:8]), " ".join(halves[8:]), "#1 4d6k3"])
check("Its fingerprint:\n  " + start_print in two and strip_time(actions(two)[1][0]) == strip_time(want[1][0]),
      "a key typed over two lines, as shown, checks the same")
other = run(["fair check " + " ".join(halves[:8]), "table forest"])
check("not the other 8 of the key" in other and "Forest encounters" in other,
      "anything else on the next line drops the half key and runs as usual")
one = run(["fair check " + key_words, "#1"])
check("#1 what?" in one, "a bare #1 asks for the action")
twice = run(["fair", "fair check " + key_words, "fair end"])
check("a live session is running" in twice, "checking can't replace a live session (its key would be lost)")

print("fair mode on the PC console: %d checks, %d failures" % (checks, failures))
sys.exit(failures != 0)
