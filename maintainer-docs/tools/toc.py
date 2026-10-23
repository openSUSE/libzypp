#!/usr/bin/env python3
"""Generate/refresh the table of contents of every chapter.

The TOC is written between TOC_BEGIN/TOC_END markers. If a chapter has no TOC
yet, it is inserted right before the first level-2 heading. It lists level-2
and level-3 headings. README.md is skipped.

Usage:  tools/toc.py [--check] [files...]
  --check  only report chapters whose TOC is missing or outdated (exit 1)
"""
import sys
from pathlib import Path

from mdlib import DOCS, TOC_BEGIN, TOC_END, headings, strip_code


def build_toc(lines):
    items = ["## Contents", ""]
    hs = headings(None, lines)[1:]  # skip the chapter title
    top = 1 if any(l == 1 for l, _, _ in hs) else 2  # chapters with "# Part X" groups
    for level, text, anchor in hs:
        if level > top + 1 or text == "Contents":
            continue
        indent = "  " * (level - top)
        items.append(f"{indent}- [{text}](#{anchor})")
    return "\n".join([TOC_BEGIN] + items + [TOC_END])


def _blankless(text):
    return [line for line in text.split("\n") if line.strip()]


def apply(path, check=False):
    text = path.read_text(encoding="utf-8")
    if TOC_BEGIN in text:
        pre, rest = text.split(TOC_BEGIN, 1)
        if TOC_END not in rest:
            sys.exit(f"{path.name}: TOC start marker without end marker {TOC_END!r}")
        _, post = rest.split(TOC_END, 1)
        stripped = pre + post.lstrip("\n")
    else:
        stripped = text
    # compute the TOC on the document without the old TOC (it has its own heading)
    lines = stripped.split("\n")
    toc = build_toc(lines)
    for no, line in strip_code(lines):  # fence-aware (``` and ~~~)
        i = no - 1
        if i > 0 and (line.startswith("## ") or line.startswith("# ")):
            new = "\n".join(lines[:i]) + "\n" + toc + "\n\n" + "\n".join(lines[i:])
            break
    else:
        new = stripped.rstrip("\n") + "\n\n" + toc + "\n"
    if new == text:
        return False
    if TOC_BEGIN in text and _blankless(text) == _blankless(new):
        return False  # only blank lines differ (e.g. a Markdown formatter): keep the file as is
    if not check:
        path.write_text(new, encoding="utf-8")
    return True


def main(argv):
    check = "--check" in argv
    files = [Path(a) for a in argv if not a.startswith("--")] or sorted(DOCS.glob("[0-9]*.md"))
    files = [f for f in files if f.name != "README.md"]
    changed = [f.name for f in files if apply(f, check)]
    for name in changed:
        print(("outdated: " if check else "updated: ") + name)
    return 1 if (check and changed) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
