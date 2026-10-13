#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
measure.py - what each QD4P feature costs in flash, RAM and UF2 size, on a
real Pico 2 build.

    python3 tests/size/measure.py           # build, print the tables
    python3 tests/size/measure.py --write   # ... and put them in docs/SIZES.md
    python3 tests/size/measure.py --define QD_MAX_ROLL_DICE=100 --define ...
                                            # with smaller limits (printed only)

Needs the Pico SDK (PICO_SDK_PATH set) and arm-none-eabi-gcc on the PATH:
the same as building the console. Takes a minute or two.

HOW IT MEASURES
    Each program in this folder uses one feature and prints its results
    over USB, so the compiler can't optimise the feature away. base.c uses
    no QD4P at all: USB serial and printf. A feature's cost is its program
    minus base: what adding that feature to an app adds to it.

    Flash and RAM come from the linker's map file (tools/size_report.py).
    "Flash added" is everything the feature brings: QD4P's own code, and
    the pieces of the Pico SDK and C library it calls (the hardware random
    source, floating-point maths). "QD4P's RAM" is the library's own
    buffers; the structures a program declares (a qd_roll_t, a qd_result_t)
    are the program's, and docs/SIZES.md lists their sizes.
    The UF2 file is about twice the flash it fills: each 512-byte UF2 block
    carries 256 bytes of program, so it grows in steps of 512 bytes.
"""
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
BUILD = os.path.join(HERE, "build")
sys.path.insert(0, os.path.join(ROOT, "tools"))
import size_report  # noqa: E402

# (program, what it does), in the order of the table
PROGRAMS = [
    ("rng",            "the generator: `qd_rng_between` for a d20 and a d6"),
    ("dice",           "single dice: a d20, a weighted custom die, a Fudge die, d% with a bonus die"),
    ("deck",           "decks: poker, the Savage Worlds action deck and tarot, dealt and named"),
    ("engine",         "the roll engine, rolls built in code: 4d20, 2d6 ie, 4d6 k3, 10d10 t8"),
    ("systems",        "the engine plus game systems, built in code and judged: 4d20, sw8, 2d6 ie, atk adv vs 10, coc 50 vs hard"),
    ("parser",         "the same five rolls typed as notation: `qd_parse`"),
    ("rerolls",        "the engine plus every kind of reroll after the roll"),
    ("odds",           "the engine plus `qd_odds`: exact and simulated"),
    ("stats",          "the engine plus stats for nerds and the fairness check"),
    ("text",           "dice inside text: `qd_expand(\"{1d4+1} wolves ...\")` (brings the parser)"),
    ("tables_own",     "two 12-entry tables written into the app: dice built in code, `qd_range_find`"),
    ("tables_example", "the same two tables in the example table system (`examples/tables`), one with {dice} in its text"),
    ("fair",           "the engine plus fair mode: fingerprint, numbered rolls, the key"),
]


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        sys.exit("failed: %s\n%s%s" % (" ".join(cmd), r.stdout[-3000:], r.stderr[-3000:]))
    return r.stdout


def kb(n):
    return "%.1f KB" % (n / 1024) if abs(n) >= 1024 else "%d B" % n


def main():
    global BUILD
    defines = [sys.argv[i + 1] for i, a in enumerate(sys.argv[:-1]) if a == "--define"]
    if defines:
        BUILD = os.path.join(HERE, "build-defines")
    os.makedirs(BUILD, exist_ok=True)
    run([sys.executable, os.path.join(ROOT, "examples/tables/tables.py"), "build",
         os.path.join(HERE, "size_tables.txt"), "--c", os.path.join(BUILD, "size_tables.h"),
         "--name", "size_tables"])
    run(["cmake", "-S", HERE, "-B", BUILD, "-DPICO_BOARD=pico2", "-DQD_DEFINES=" + ";".join(defines)])
    run(["cmake", "--build", BUILD, "-j8"])

    def measure(p):
        s = size_report.summary(os.path.join(BUILD, p + ".elf.map"))
        return {"uf2": os.path.getsize(os.path.join(BUILD, p + ".uf2")), "flash": s["total"][0],
                "ram": s["total"][1], "qd": s["groups"].get("qd4p", [0, 0]),
                "app": s["groups"].get("program", [0, 0]), "files": s["qd4p_files"]}

    base = measure("base")
    rows = []
    for p, what in PROGRAMS:
        m = measure(p)
        files = ", ".join("%s %s" % (f.replace("qd_", ""), kb(v[0]))
                          for f, v in sorted(m["files"].items(), key=lambda kv: -kv[1][0]) if v[0] >= 256)
        rows.append((p, what, m["uf2"] - base["uf2"], m["flash"] - base["flash"],
                     m["ram"] - base["ram"], files, m))

    version = re.search(r"QD_VERSION_MAJOR (\d+).*?QD_VERSION_MINOR (\d+).*?QD_VERSION_PATCH (\d+)",
                        open(os.path.join(ROOT, "qd4p/qd4p.h")).read(), re.S).groups()
    gcc = run(["arm-none-eabi-gcc", "-dumpversion"]).strip()
    out = ["Measured with QD4P %s, Pico SDK for `pico2`, arm-none-eabi-gcc %s, Release build. "
           "The baseline program (USB serial and `printf`, no QD4P) is %s of flash, a %s UF2 file."
           % (".".join(version), gcc, kb(base["flash"]), "{:,}".format(base["uf2"]) + "-byte")
           + (" Settings changed: `%s`." % "`, `".join(defines) if defines else ""),
           "",
           "| Program | What it uses | Flash added | of which QD4P | QD4P's RAM | UF2 added | QD4P's flash, by file |",
           "|---|---|---:|---:|---:|---:|---|"]
    for p, what, duf2, dfl, dram, files, m in rows:
        out.append("| `%s` | %s | %s | %s | %s | %s | %s |" % (p, what, kb(dfl), kb(m["qd"][0]), kb(m["qd"][1]),
                                                            "{:,}".format(duf2), files or "-"))
    table = "\n".join(out)
    print(table)
    print()
    print(json.dumps({p: {"flash": d, "qd_flash": m["qd"][0], "qd_ram": m["qd"][1], "uf2": u}
                      for p, _, u, d, r, _, m in rows}))

    if "--write" in sys.argv and not defines:
        path = os.path.join(ROOT, "docs/SIZES.md")
        doc = open(path, encoding="utf-8").read()
        a, b = "<!-- measure.py: begin -->", "<!-- measure.py: end -->"
        if a not in doc or b not in doc:
            sys.exit("docs/SIZES.md has no %s ... %s markers" % (a, b))
        doc = doc[:doc.index(a) + len(a)] + "\n" + table + "\n" + doc[doc.index(b):]
        open(path, "w", encoding="utf-8").write(doc)
        print("docs/SIZES.md updated")


if __name__ == "__main__":
    main()
