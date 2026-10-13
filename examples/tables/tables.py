#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
tables.py - random tables for the example table system (table.c): check
them, build the asset the device reads, and turn an asset back into text.

    python3 examples/tables/tables.py check  my_tables.txt
    python3 examples/tables/tables.py build  my_tables.txt -o tables.bin
    python3 examples/tables/tables.py build  my_tables.txt --c tables.h --name my_tables
    python3 examples/tables/tables.py dump   tables.bin            (text back, to stdout)

THE TEXT FORMAT (full guide: README.md beside this file)

    # comments start with # at the start of a line
    table Forest encounters
    roll 2d6                      (optional: 1dN, N the highest range, if left out)
    2      Owlbear
    3-4    {1d4+1} wolves          {notation} is rolled each time
    5-9    Nothing stirs
    10-11  Bandits: [Bandit leaders]   [Name] rolls on another table
    12     Roll twice              roll twice / roll N times on this table
    13+    Something big           13+ is 13 or more, 1- is 1 or less
           (a line that starts with a space carries on the entry above)

Ranges must not overlap. For simple rolls (2d6, d20, d%, 3d6+2, dd66) the
check also reports values no entry covers.
"""
import argparse
import re
import struct
import sys

MAGIC = b"QDT1"
KIND_TEXT, KIND_AGAIN = 0, 1
I32_MIN, I32_MAX = -(2 ** 31), 2 ** 31 - 1
AGAIN = re.compile(r"^roll\s+(twice|thrice|three times|(\d+)\s+times)\b", re.I)
ENTRY = re.compile(r"^\s*(\d+)\s*(?:(-)\s*(\d+)|(\+)|(-))?\s+(\S.*)$")


class TableError(Exception):
    pass


class Table:
    def __init__(self, name, line):
        self.name, self.line, self.roll, self.entries = name, line, None, []


# ---------------------------------------------------------------------------
# Reading the text
# ---------------------------------------------------------------------------
def number(tok):
    return 100 if tok == "00" else int(tok)


def parse_text(text):
    """Text -> list of Table. Raises TableError on a line it can't read."""
    tables, cur = [], None
    for n, raw in enumerate(text.splitlines(), 1):
        line = raw.rstrip()
        if not line.strip() or line.lstrip().startswith("#") and not line.startswith(" "):
            continue
        low = line.strip().lower()
        if low.startswith("table ") or low == "table":
            name = line.strip()[5:].strip()
            if not name:
                raise TableError("line %d: a table needs a name" % n)
            cur = Table(name, n)
            tables.append(cur)
            continue
        if cur is None:
            raise TableError("line %d: start with 'table <name>'" % n)
        if low.startswith("roll ") and not cur.entries and cur.roll is None and not AGAIN.match(low):
            cur.roll = line.strip()[5:].strip()
            continue
        m = ENTRY.match(line)
        if m:
            a, dash, b, plus, minus, body = m.groups()
            lo = number(a)
            if dash:
                hi = number(b)
            elif plus:
                hi = I32_MAX
            elif minus:
                lo, hi = I32_MIN, number(a)
            else:
                hi = lo
            if lo > hi:
                raise TableError("line %d: the range %s-%s runs backwards" % (n, a, b))
            cur.entries.append([lo, hi, body.strip(), n])
            continue
        if raw[:1] in (" ", "\t") and cur.entries:          # carries on the entry above
            cur.entries[-1][2] += " " + line.strip()
            continue
        raise TableError("line %d: expected a range and text, like '3-4  Wolves': %s" % (n, line.strip()))
    return tables


def entry_kind(body):
    m = AGAIN.match(body.strip())
    if not m:
        return KIND_TEXT, 0
    w = m.group(1).lower()
    times = 2 if w == "twice" else 3 if w in ("thrice", "three times") else int(m.group(2))
    return KIND_AGAIN, max(1, min(times, 9))


def possible_values(roll):
    """Every value simple notation can roll, or None when it isn't simple."""
    r = roll.replace(" ", "").lower()
    m = re.fullmatch(r"1?dd([1-9]{1,4})", r)
    if m:                                   # one set of digit dice: only real digit combinations
        vals = [0]
        for s in m.group(1):
            vals = [v * 10 + d for v in vals for d in range(1, int(s) + 1)]
        return vals
    rr = roll_range(r)
    return list(range(rr[0], rr[1] + 1)) if rr else None


def roll_range(roll):
    """(lowest, highest) for simple notation, or None when it isn't simple."""
    r = roll.replace(" ", "").lower()
    m = re.fullmatch(r"(\d*)d(\d+|%|f)(?:([+-])(\d+))?", r)
    if not m:
        return None
    n = int(m.group(1) or 1)
    k = int(m.group(4) or 0) * (-1 if m.group(3) == "-" else 1)
    if m.group(2) == "f":
        return -n + k, n + k
    s = 100 if m.group(2) == "%" else int(m.group(2))
    return n + k, n * s + k


def check(tables):
    """Returns (errors, warnings), each a list of strings."""
    errors, warnings, names = [], [], {}
    for t in tables:
        key = t.name.lower()
        if key in names:
            errors.append("table '%s' (line %d) has the same name as line %d" % (t.name, t.line, names[key]))
        names[key] = t.line
        if not t.entries:
            errors.append("table '%s' (line %d) has no entries" % (t.name, t.line))
    for t in tables:
        if not t.entries:
            continue
        if t.roll is None:
            lows = min(e[0] for e in t.entries)
            if lows != 1 or max(e[1] for e in t.entries) >= I32_MAX:
                errors.append("table '%s': without a roll line, ranges must start at 1 and end at a number"
                              % t.name)
                continue
            t.roll = "1d%d" % max(e[1] for e in t.entries)
        ents = sorted(t.entries, key=lambda e: e[0])
        for a, b in zip(ents, ents[1:]):
            if b[0] <= a[1]:
                errors.append("table '%s' line %d: overlaps line %d" % (t.name, b[3], a[3]))
        vals = possible_values(t.roll)
        if vals:
            missing = [v for v in vals if not any(e[0] <= v <= e[1] for e in t.entries)]
            if missing:
                show = ", ".join(map(str, missing[:8])) + (" ..." if len(missing) > 8 else "")
                warnings.append("table '%s': %s can roll %s, with no entry" % (t.name, t.roll, show))
        for lo, hi, body, n in t.entries:
            depth, i = 0, 0
            while i < len(body):
                c = body[i]
                if c == "\\":
                    i += 2
                    continue
                if c == "{":
                    j = body.find("}", i)
                    if j < 0:
                        errors.append("table '%s' line %d: a '{' with no '}'" % (t.name, n))
                        break
                    if not body[i + 1:j].strip():
                        errors.append("table '%s' line %d: an empty {}" % (t.name, n))
                    i = j
                elif c == "[":
                    j = body.find("]", i)
                    if j < 0:
                        errors.append("table '%s' line %d: a '[' with no ']'" % (t.name, n))
                        break
                    link = body[i + 1:j].strip()
                    if link.lower() not in names:
                        errors.append("table '%s' line %d: no table called '%s'" % (t.name, n, link))
                    i = j
                i += 1
            if len(body.encode("utf-8")) > 1000:
                errors.append("table '%s' line %d: entry text over 1000 bytes" % (t.name, n))
    return errors, warnings


# ---------------------------------------------------------------------------
# The asset: little-endian, strings NUL-terminated so the device can use
# them in place.
#   "QDT1"  u16 table count  u16 0  u32 offset of each table
#   table:  name\0  roll\0  u16 entry count  entries
#   entry:  i32 low  i32 high  u8 kind  u8 times  text\0
# ---------------------------------------------------------------------------
def default_roll(t):
    """1dN for a table without a roll line, N its highest range."""
    if t.roll is None and t.entries:
        t.roll = "1d%d" % max(e[1] for e in t.entries)
    return t.roll


def build(tables):
    for t in tables:
        default_roll(t)
    head = MAGIC + struct.pack("<HH", len(tables), 0)
    body, offsets = b"", []
    base = len(head) + 4 * len(tables)
    for t in tables:
        offsets.append(base + len(body))
        rec = t.name.encode("utf-8") + b"\0" + t.roll.encode("utf-8") + b"\0"
        rec += struct.pack("<H", len(t.entries))
        for lo, hi, text, _ in t.entries:
            kind, times = entry_kind(text)
            rec += struct.pack("<iiBB", lo, hi, kind, times) + text.encode("utf-8") + b"\0"
        body += rec
    return head + b"".join(struct.pack("<I", o) for o in offsets) + body


def read_asset(data):
    """Asset bytes -> list of Table (for dump and tests)."""
    if data[:4] != MAGIC:
        raise TableError("not a QD4P table asset (no QDT1 at the start)")
    count = struct.unpack_from("<H", data, 4)[0]
    tables = []
    for k in range(count):
        at = struct.unpack_from("<I", data, 8 + 4 * k)[0]

        def cstr(p):
            e = data.index(b"\0", p)
            return data[p:e].decode("utf-8"), e + 1
        name, at = cstr(at)
        roll, at = cstr(at)
        n = struct.unpack_from("<H", data, at)[0]
        at += 2
        t = Table(name, 0)
        t.roll = roll
        for _ in range(n):
            lo, hi, kind, times = struct.unpack_from("<iiBB", data, at)
            text, at = cstr(at + 10)
            t.entries.append([lo, hi, text, 0])
        tables.append(t)
    return tables


def to_text(tables):
    out = []
    for t in tables:
        out.append("table %s" % t.name)
        out.append("roll %s" % t.roll)
        for lo, hi, text, _ in t.entries:
            if lo == I32_MIN:
                r = "%d-" % hi
            elif hi == I32_MAX:
                r = "%d+" % lo
            elif lo == hi:
                r = "%d" % lo
            else:
                r = "%d-%d" % (lo, hi)
            out.append("%-7s %s" % (r, text))
        out.append("")
    return "\n".join(out)


def c_header(data, name):
    lines = ["/* SPDX-License-Identifier: MIT-0 */",
             "/* SPDX-AI-Disclosure: ai-generated */",
             "/* SPDX-AI-Model: claude-opus-5-5 */",
             "/* SPDX-AI-Provider: Anthropic */",
             "/* Generated by examples/tables/tables.py: a random-table asset. Do not edit;",
             " * edit the text file and build again. */",
             "#include <stdint.h>",
             "static const uint8_t %s[%d] = {" % (name, len(data))]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join("0x%02x" % b for b in data[i:i + 16]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


def load(path):
    with open(path, encoding="utf-8") as f:
        tables = parse_text(f.read())
    errors, warnings = check(tables)
    for w in warnings:
        print("warning: " + w, file=sys.stderr)
    if errors:
        for e in errors:
            print("error: " + e, file=sys.stderr)
        raise SystemExit(1)
    return tables


def main(argv=None):
    ap = argparse.ArgumentParser(description="QD4P random tables: check, build, dump")
    sub = ap.add_subparsers(dest="cmd", required=True)
    c = sub.add_parser("check", help="check a table text file")
    c.add_argument("text")
    b = sub.add_parser("build", help="build the asset (and/or a C header)")
    b.add_argument("text")
    b.add_argument("-o", "--out", help="binary asset to write")
    b.add_argument("--c", dest="c_out", help="C header to write")
    b.add_argument("--name", default="qd_tables_data", help="the array's name in the C header")
    d = sub.add_parser("dump", help="an asset back to text")
    d.add_argument("asset")
    a = ap.parse_args(argv)

    if a.cmd == "check":
        tables = load(a.text)
        print("%d tables, %d entries: OK" % (len(tables), sum(len(t.entries) for t in tables)))
    elif a.cmd == "build":
        data = build(load(a.text))
        if a.out:
            open(a.out, "wb").write(data)
        if a.c_out:
            open(a.c_out, "w").write(c_header(data, a.name))
        if not a.out and not a.c_out:
            raise SystemExit("build: give -o and/or --c")
        print("%d bytes" % len(data))
    else:
        sys.stdout.write(to_text(read_asset(open(a.asset, "rb").read())))


if __name__ == "__main__":
    main()
