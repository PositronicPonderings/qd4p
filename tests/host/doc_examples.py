#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
doc_examples.py - build and run every example in the manual, and check that
what it prints is what the manual says it prints.

    python3 tests/host/doc_examples.py            # check
    python3 tests/host/doc_examples.py --update   # write the outputs into the manual
    python3 tests/host/doc_examples.py roll_      # only examples whose name starts so

WHAT COUNTS AS AN EXAMPLE
    A fenced code block in docs/manual whose info string starts with
    "c example=NAME":

        ```c example=d20
        printf("%ld\\n", (long)qd_rng_between(1, 20));
        ```

        ```text output
        14
        ```

    The block right after it, if it is "text output", is what running it
    prints. --update writes or refreshes those blocks; a check fails if one
    is missing or different, so the manual can't quietly rot.

    A block without "int main" is the body of main(), with <stdio.h>,
    <string.h>, <stdlib.h> and qd4p.h already included. A block with
    main() is a whole program; unless it starts with its own #includes,
    it gets those four too.

OPTIONS after the name
    file=PATH       the block must be exactly this file of the repo (an
                    example program); the file is what gets built
    input=TEXT      what to type at it (\\n for Enter), for programs that
                    wait for keys; at the end of the input they stop
    tables          with the example table system (examples/tables) and
                    the console's sample tables, sample_tables[]
    compile-only    compile it, don't run it
    no-output       run it, but it shows no output block

CONSOLE TRANSCRIPTS
    A block whose info string is "console" (optionally "console seed=N")
    is a session at the qd4p console: lines starting with "> " are typed
    in, everything else is what the console prints. The console, built for
    the PC, is run with those lines and must print exactly that (timings,
    "41 us" or "3 ms", are compared as "N us", "N ms"). --update rewrites
    the printed part. With seed=N, "seed N" is typed first, unseen.

THE DICE
    Examples are linked with doc_platform.c, whose "hardware" randomness is
    the same every run, so their output is too. Pico programs build against
    the stand-in Pico headers in examples/pc.
"""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
MANUAL = os.path.join(ROOT, "docs", "manual")
BUILD = os.path.join(HERE, "build", "doc")
LIB = os.path.join(ROOT, "qd4p")
CC = os.environ.get("CC", "cc")
FENCE = re.compile(r"^```c example=(\S+)([^\n]*)\n(.*?)^```[ \t]*\n", re.S | re.M)
CONSOLE = re.compile(r"^```console([^\n]*)\n(.*?)^```[ \t]*\n", re.S | re.M)
TIMING = re.compile(r"\b\d+ (ms|us)\b")
OUTPUT = re.compile(r"(\s*\n)?```text output[ \t]*\n(.*?)^```[ \t]*\n", re.S | re.M)  # .match(text, pos): from pos

TEMPLATE = """#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "qd4p.h"
%s
int main(void)
{
%s
    return 0;
}
"""


def options(text):
    """'file=x input="a b" tables' -> dict."""
    opts = {}
    for m in re.finditer(r'(\w[\w-]*)(?:=("([^"]*)"|\S+))?', text):
        opts[m.group(1)] = m.group(3) if m.group(3) is not None else (m.group(2) or True)
    return opts


def find_examples():
    found = []
    for root, _dirs, files in os.walk(MANUAL):
        for f in sorted(files):
            if f.endswith(".md"):
                path = os.path.join(root, f)
                text = open(path, encoding="utf-8").read()
                for m in FENCE.finditer(text):
                    out = OUTPUT.match(text, m.end())
                    found.append({"name": m.group(1), "opts": options(m.group(2)), "code": m.group(3),
                                  "path": path, "end": m.end(),
                                  "output": out.group(2) if out else None,
                                  "out_span": (m.end(), out.end()) if out else None})
    return found


def find_transcripts():
    found = []
    for root, _dirs, files in os.walk(MANUAL):
        for f in sorted(files):
            if f.endswith(".md"):
                path = os.path.join(root, f)
                text = open(path, encoding="utf-8").read()
                for m in CONSOLE.finditer(text):
                    found.append({"opts": options(m.group(1)), "body": m.group(2), "path": path,
                                  "span": (m.start(2), m.end(2))})
    return found


def console_run(exe, typed, seed):
    """What the console prints for these lines, as a transcript."""
    stdin = ("seed %s\n" % seed if seed else "") + "".join(t + "\n" for t in typed)
    r = run([exe], input=stdin, timeout=60)
    out = r.stdout.replace("\r", "")
    out = out[out.index("> "):]                         # drop the banner
    if seed:
        out = out[out.index("\n> ", out.index("seed ")) + 1:]   # and the hidden seed line
    if out.endswith("> \n"):
        out = out[:-3]
    return tidy(out)


def tidy(s):
    lines = [line.rstrip() for line in s.replace("\r", "").split("\n")]
    while lines and not lines[-1]:
        lines.pop()
    return "\n".join(lines) + "\n" if lines else ""


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    update = "--update" in sys.argv
    os.makedirs(BUILD, exist_ok=True)
    cflags = ["-std=c11", "-O1", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-unused-variable",
              "-D_POSIX_C_SOURCE=200809L", "-I" + LIB, "-I" + os.path.join(ROOT, "examples"),
              "-I" + os.path.join(ROOT, "examples", "pc"), "-I" + os.path.join(ROOT, "examples", "tables")]

    # The library once, with the repeatable "hardware".
    objs = []
    sources = [os.path.join(LIB, f) for f in sorted(os.listdir(LIB))
               if f.endswith(".c") and not f.endswith(("_pico.c", "_host.c"))]
    sources += [os.path.join(HERE, "doc_platform.c"), os.path.join(ROOT, "examples/pc/pc_hardware.c")]
    for src in sources:
        o = os.path.join(BUILD, os.path.basename(src)[:-2] + ".o")
        r = run([CC, *cflags, "-c", src, "-o", o])
        if r.returncode:
            sys.exit("library build failed: %s\n%s" % (src, r.stderr))
        objs.append(o)
    table_o = os.path.join(BUILD, "table.o")
    r = run([CC, *cflags, "-c", os.path.join(ROOT, "examples/tables/table.c"), "-o", table_o])
    if r.returncode:
        sys.exit("table.c: " + r.stderr)

    # The console, for transcripts: the same console.c, with repeatable dice.
    console = os.path.join(BUILD, "qd4p_console")
    r = run([CC, *cflags, "-o", console, os.path.join(ROOT, "examples/console.c"),
             os.path.join(ROOT, "examples/tables/table.c"), *objs, "-lm"])
    if r.returncode:
        sys.exit("console build failed:\n" + r.stderr[-3000:])

    examples = [e for e in find_examples() if not args or any(e["name"].startswith(a) for a in args)]
    names = {}
    failures, edits = [], {}
    for ex in examples:
        name, opts, code = ex["name"], ex["opts"], ex["code"]
        where = "%s (%s)" % (name, os.path.relpath(ex["path"], ROOT))
        if name in names:
            failures.append("%s: the name is used twice (also %s)" % (where, names[name]))
            continue
        names[name] = os.path.relpath(ex["path"], ROOT)
        src = os.path.join(BUILD, name + ".c")
        if "file" in opts:
            path = os.path.join(ROOT, opts["file"])
            if not os.path.exists(path) or open(path, encoding="utf-8").read() != code:
                failures.append("%s: the block isn't the same as %s" % (where, opts["file"]))
                continue
            src = path
        else:
            extra = '#include "table.h"\n#include "tables/sample_tables.h"\n' if "tables" in opts else ""
            body = code if "int main" in code else TEMPLATE % (extra, "\n".join(
                "    " + line if line else "" for line in code.rstrip("\n").split("\n")))
            if "int main" in code and not code.lstrip().startswith("#"):
                body = '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include "qd4p.h"\n' + body
            if "int main" in code and extra:
                body = extra + body
            open(src, "w", encoding="utf-8").write(body)
        exe = os.path.join(BUILD, name)
        link = [table_o] if "tables" in opts or "table.h" in code else []
        r = run([CC, *cflags, "-o", exe, src, *link, *objs, "-lm"])
        if r.returncode or "warning" in r.stderr:
            failures.append("%s: doesn't compile cleanly:\n%s" % (where, r.stderr[-2000:]))
            continue
        if "compile-only" in opts:
            continue
        stdin = opts.get("input", "")
        stdin = stdin.replace("\\n", "\n") if isinstance(stdin, str) else ""
        r = run([exe], input=stdin, timeout=60)
        if r.returncode:
            failures.append("%s: exited with %d\n%s" % (where, r.returncode, r.stdout[-1000:] + r.stderr[-1000:]))
            continue
        got = tidy(r.stdout)
        if "no-output" in opts:
            continue
        if ex["output"] is None or tidy(ex["output"]) != got:
            if update:
                edits.setdefault(ex["path"], []).append((ex, got))
            else:
                failures.append("%s: prints something else than the manual shows:\n--- manual\n%s--- printed\n%s"
                                % (where, ex["output"] or "(no output block)\n", got))

    for path, items in edits.items():
        text = open(path, encoding="utf-8").read()
        for ex, got in sorted(items, key=lambda i: -i[0]["end"]):
            block = "\n```text output\n" + got + "```\n"
            a, b = ex["out_span"] if ex["out_span"] else (ex["end"], ex["end"])
            text = text[:a] + block + text[b:]
        open(path, "w", encoding="utf-8").write(text)
        print("updated %d output%s in %s" % (len(items), "s" if len(items) > 1 else "", os.path.relpath(path, ROOT)))

    # transcripts after the example outputs are written: their places move
    transcripts = find_transcripts() if not args else []
    tedits = {}
    for t in transcripts:
        typed = [line[2:] for line in t["body"].split("\n") if line.startswith("> ")]
        got = console_run(console, typed, t["opts"].get("seed"))
        want = tidy(t["body"])
        if TIMING.sub(r"N \1", got) != TIMING.sub(r"N \1", want):
            if update:
                tedits.setdefault(t["path"], []).append((t, got))
            else:
                failures.append("console transcript in %s, starting '%s':\n--- manual\n%s--- printed\n%s"
                                % (os.path.relpath(t["path"], ROOT), typed[0] if typed else "", want, got))
    for path, items in tedits.items():
        text = open(path, encoding="utf-8").read()
        for t, got in sorted(items, key=lambda i: -i[0]["span"][0]):
            a, b = t["span"]
            text = text[:a] + got + text[b:]
        open(path, "w", encoding="utf-8").write(text)
        print("updated %d console transcript%s in %s" % (len(items), "s" if len(items) > 1 else "",
                                                       os.path.relpath(path, ROOT)))

    for f in failures:
        print("FAIL  " + f)
    print("manual examples: %d programs and %d console sessions checked, %d failures"
          % (len(examples), len(transcripts), len(failures)))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
