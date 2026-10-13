#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
size_report.py - how much flash and RAM each part of a Pico program uses,
and how much of it is QD4P, read from the linker's map file.

EXAMPLE
    python3 tools/size_report.py build/examples/qd4p_console.elf.map

    For tests/size/systems.c (five rolls built in code, and judged):

                                           flash       RAM
        qd4p:                              15171     16038
            qd_roll                         9213     16012
            qd_sys                          4429         0
            qd_rng                           888        26
            ...
        TOTAL                              48993     25542

WHERE THE MAP FILE COMES FROM
    The Pico SDK build already writes one, beside the program:
    build/<program>.elf.map. The linker lists in it every piece of code and
    data it placed, with its size and the file it came from. This script
    adds those up per source file.

WHAT THE COLUMNS MEAN
    flash   code (.text) and constant data (.rodata), plus the starting
            values of initialised variables (.data), which are copied to RAM
            at start-up
    RAM     variables: initialised (.data) and zeroed (.bss). The stack and
            the heap aren't counted: they aren't in the map.

ONLY WHAT IS LINKED IS COUNTED
    The Pico SDK compiles every function into its own section and the linker
    drops the ones nothing calls (-ffunction-sections, --gc-sections). So
    the figures are for THIS program: a program that never deals a card has
    no deck code in it, whatever the library contains. That is QD4P's rule:
    you only pay for what you use. docs/SIZES.md lists what each feature
    costs.

    python3 tools/size_report.py --json build/x.elf.map   prints the same
    figures as JSON, for scripts (tests/size/measure.py uses it).
"""

import json
import re
import sys
from collections import defaultdict

# One placed input section, in either of the two layouts GNU ld writes:
#   " .text.foo      0x10001234      0x1a4 path/lib.a(file.c.obj)"
#   " .text.foo\n                0x10001234      0x1a4 path/lib.a(file.c.obj)"
# (the second when the section's name is too long to share the line)
ENTRY = re.compile(r"^\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(\S.*)$")
NAMED = re.compile(r"^ (\.\S+|COMMON)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(\S.*)$")
NAME_ONLY = re.compile(r"^ (\.\S+|COMMON)\s*$")


def kind(section):
    """'flash', 'ram' or 'both', from an input section's name; None for
    debug information and other bookkeeping that isn't in the program."""
    if section == "COMMON" or section.startswith((".bss", ".noinit", ".uninitialized")):
        return "ram"
    if section.startswith(".data") or section.startswith(".time_critical"):
        return "both"       # stored in flash, copied to RAM at start-up
    if section.startswith((".text", ".rodata", ".ARM", ".init", ".fini", ".boot", ".binary_info",
                           ".embedded", ".flashdata", ".vectors", ".eh_frame", ".gnu", ".picobin")):
        return "flash"
    return None


def source_of(path):
    """('group', 'file') for an object file's path: ('qd4p', 'qd_roll'),
    ('pico-sdk', 'stdio'), ('program', 'main') ..."""
    m = re.search(r"([^/\\]+)\.a\(([^)]+)\)", path)
    if m:
        lib, obj = m.group(1), m.group(2)
        lib = lib[3:] if lib.startswith("lib") else lib      # libqd4p -> qd4p
        obj = re.sub(r"\.(c|cpp|S)\.obj$|\.(c|cpp|S)\.o$|\.obj$|\.o$", "", obj)
        if lib in ("c", "g", "m", "gcc", "nosys", "stdc++", "c_nano", "g_nano"):
            lib = "C library"
        return lib, obj
    obj = re.sub(r".*[/\\]", "", path)
    obj = re.sub(r"\.(c|cpp|S)\.obj$|\.(c|cpp|S)\.o$|\.obj$|\.o$", "", obj)
    if "pico-sdk" in path or ".pico-sdk" in path or "/sdk/" in path or "pico_" in path:
        return "pico-sdk", obj
    return "program", obj


def parse(path):
    """{(group, file): [flash, ram]} for every placed section in the map."""
    sizes = defaultdict(lambda: [0, 0])
    in_map = False
    pending = None
    with open(path, errors="replace") as fh:
        for line in fh:
            if line.startswith("Linker script and memory map"):
                in_map = True
                continue
            if not in_map:
                continue                        # skips "Discarded input sections"
            m = NAMED.match(line)
            if m:
                sec, size, obj = m.group(1), int(m.group(3), 16), m.group(4)
                pending = None
            else:
                m1 = NAME_ONLY.match(line)
                if m1:
                    pending = m1.group(1)
                    continue
                m2 = ENTRY.match(line)
                if not (m2 and pending):
                    pending = None
                    continue
                sec, size, obj = pending, int(m2.group(2), 16), m2.group(3)
                pending = None
            if size == 0 or obj.startswith("*"):
                continue
            k = kind(sec)
            if k is None:
                continue
            key = source_of(obj.strip())
            if k in ("flash", "both"):
                sizes[key][0] += size
            if k in ("ram", "both"):
                sizes[key][1] += size
    return sizes


def summary(path):
    """The figures as a dictionary: per group, and QD4P per file."""
    sizes = parse(path)
    groups = defaultdict(lambda: [0, 0])
    files = {}
    for (g, f), (fl, ram) in sizes.items():
        groups[g][0] += fl
        groups[g][1] += ram
        if g == "qd4p":
            files[f] = [fl, ram]
    return {"groups": dict(groups), "qd4p_files": files,
            "total": [sum(v[0] for v in groups.values()), sum(v[1] for v in groups.values())]}


def main():
    args = [a for a in sys.argv[1:] if a != "--json"]
    if len(args) != 1:
        sys.exit(__doc__)
    s = summary(args[0])
    if not s["groups"]:
        sys.exit("No placed sections found. Is this a GNU ld map file?")
    if "--json" in sys.argv:
        print(json.dumps(s, indent=1, sort_keys=True))
        return

    order = ["qd4p", "program", "pico-sdk", "C library"]
    order += sorted(g for g in s["groups"] if g not in order)
    print("%-34s %9s %9s" % ("", "flash", "RAM"))
    for g in order:
        if g not in s["groups"]:
            continue
        fl, ram = s["groups"][g]
        print("%-34s %9d %9d" % (g + ":", fl, ram))
        if g == "qd4p":
            for f, (ffl, fram) in sorted(s["qd4p_files"].items(), key=lambda r: -r[1][0]):
                print("    %-30s %9d %9d" % (f, ffl, fram))
    fl, ram = s["total"]
    print("%-34s %9d %9d" % ("TOTAL", fl, ram))
    q = s["groups"].get("qd4p", [0, 0])
    print("\nQD4P: %.1f KB of flash and %.1f KB of RAM of this program's %.1f KB and %.1f KB"
          % (q[0] / 1024, q[1] / 1024, fl / 1024, ram / 1024))
    print("(RAM excludes the stack and heap; docs/SIZES.md says what each feature costs)")


if __name__ == "__main__":
    main()
