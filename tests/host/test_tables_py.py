#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
test_tables_py.py - checks examples/tables/tables.py: the checker catches mistakes,
text -> asset -> text -> asset gives the same bytes, and the console's
built-in sample header is up to date with its text.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
sys.path.insert(0, os.path.join(ROOT, "examples", "tables"))
import tables  # noqa: E402

checks = fails = 0


def check(ok, what):
    global checks, fails
    checks += 1
    if not ok:
        fails += 1
        print("  FAIL " + what)


def errors_of(text):
    try:
        return tables.check(tables.parse_text(text))
    except tables.TableError as e:
        return [str(e)], []


for name in ("tests/host/tables_test.txt", "examples/tables/sample_tables.txt"):
    t = tables.parse_text(open(os.path.join(ROOT, name), encoding="utf-8").read())
    err, _ = tables.check(t)
    check(not err, "%s checks clean: %s" % (name, err))
    data = tables.build(t)
    again = tables.build(tables.parse_text(tables.to_text(tables.read_asset(data))))
    check(data == again, "%s: text -> asset -> text -> asset is the same bytes" % name)

sample = tables.build(tables.parse_text(open(os.path.join(ROOT, "examples/tables/sample_tables.txt"),
                                             encoding="utf-8").read()))
header = open(os.path.join(ROOT, "examples/tables/sample_tables.h"), encoding="utf-8").read()
check(tables.c_header(sample, "sample_tables") == header,
      "examples/tables/sample_tables.h is up to date (rebuild it with tables.py)")

e, w = errors_of("table A\nroll 1d6\n1-3 x\n3-6 y\n")
check(any("overlaps" in x for x in e), "overlapping ranges are an error")
e, w = errors_of("table A\nroll 1d6\n1-3 x [Nope]\n4-6 y\n")
check(any("no table called 'Nope'" in x for x in e), "a link to a missing table is an error")
e, w = errors_of("table A\nroll 1d6\n1-3 x\n")
check(not e and any("4, 5, 6" in x for x in w), "gaps are a warning: %s" % w)
e, w = errors_of("table A\n1-3 {1d4 wolves\n")
check(any("no '}'" in x for x in e), "an unclosed { is an error")
e, w = errors_of("table A\ntable A\n1 x\n")
check(any("same name" in x for x in e), "two tables with one name is an error")
e, w = errors_of("table A\nroll 1d6\nsix wolves\n")
check(any("expected a range" in x for x in e), "an entry without a range is an error")
e, w = errors_of("table A\nroll dd66\n11-36 a\n41-66 b\n")
check(not w, "dd66 only needs the real digit combinations: %s" % w)
check(tables.entry_kind("Roll twice")[0] == tables.KIND_AGAIN and tables.entry_kind("roll 4 times")[1] == 4
      and tables.entry_kind("Roll thrice, keep both")[1] == 3 and tables.entry_kind("Rolling stones")[0] == 0,
      "roll-again entries are recognised")

r = subprocess.run([sys.executable, os.path.join(ROOT, "examples/tables/tables.py"), "check", "/dev/stdin"],
                   input="table A\nroll 1d6\n1-3 x\n3-6 y\n", capture_output=True, text=True)
check(r.returncode == 1 and "overlaps" in r.stderr, "the command line exits 1 on errors")

print("table tool: %d checks, %d failures" % (checks, fails))
sys.exit(1 if fails else 0)
