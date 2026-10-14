#!/usr/bin/env python3
# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
"""
check_links.py - every relative link in the documentation leads somewhere:
the file exists, and a #section in it is a heading there.

    python3 tests/host/check_links.py

Headings become anchors the way GitHub makes them: lower case, punctuation
dropped (hyphens and underscores kept), spaces turned into hyphens, and a
-1, -2 ... added to repeats. Links to the web aren't followed.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
LINK = re.compile(r"(?<!!)\[[^\]]*\]\(([^)\s]+)\)")
SKIP = ("build", ".git", "__pycache__")


def slug(heading):
    h = re.sub(r"`|\*|\\", "", heading.strip()).lower()
    h = re.sub(r"[^\w\- ]", "", h)
    return h.replace(" ", "-")


def anchors(path):
    found, seen = set(), {}
    in_code = False
    for line in open(path, encoding="utf-8"):
        if line.startswith("```"):
            in_code = not in_code
        if in_code:
            continue
        m = re.match(r"^#{1,6} (.+)$", line.rstrip("\n"))
        if m:
            s = slug(m.group(1))
            n = seen.get(s, 0)
            seen[s] = n + 1
            found.add(s if n == 0 else "%s-%d" % (s, n))
    return found


def main():
    bad = []
    files = 0
    for root, dirs, names in os.walk(ROOT):
        dirs[:] = [d for d in dirs if not d.startswith(SKIP)]
        for name in names:
            if not name.endswith(".md"):
                continue
            path = os.path.join(root, name)
            files += 1
            text = open(path, encoding="utf-8").read()
            text = re.sub(r"^```.*?^```", "", text, flags=re.S | re.M)   # not in code blocks
            for target in LINK.findall(text):
                if re.match(r"^[a-z]+:", target):
                    continue                                          # https:, mailto:
                file_part, _, anchor = target.partition("#")
                dest = os.path.normpath(os.path.join(os.path.dirname(path), file_part)) if file_part else path
                where = os.path.relpath(path, ROOT)
                if not os.path.exists(dest):
                    bad.append("%s: %s (no such file)" % (where, target))
                elif anchor and dest.endswith(".md") and anchor not in anchors(dest):
                    bad.append("%s: %s (no such section)" % (where, target))
    for b in bad:
        print("BROKEN  " + b)
    print("links in %d documents: %d broken" % (files, len(bad)))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
