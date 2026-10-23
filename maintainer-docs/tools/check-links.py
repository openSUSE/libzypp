#!/usr/bin/env python3
"""Check links inside maintainer-docs and the source-root README.md /
CONTRIBUTING.md files (incl. zyppng/CONTRIBUTING.md).

Errors (exit 1):
  * relative link to a file that does not exist
  * '#anchor' that does not exist in the target file
Warnings:
  * '[NN §X.Y](file.md)' references whose section number has no heading
    starting with that number in the target file

Usage:  tools/check-links.py [-q]     (-q: errors only)
"""
import re
import sys
from pathlib import Path

from mdlib import DOCS, explicit_anchors, headings, links

SECTION = re.compile(r"§\s*([A-Z]?\.?[0-9]+(?:\.[0-9]+)*)")


def anchors_of(path, cache={}):
    if path not in cache:
        cache[path] = {a for _, _, a in headings(path)} | explicit_anchors(path)
    return cache[path]


def section_numbers(path, cache={}):
    if path not in cache:
        nums = set()
        for _, text, _ in headings(path):
            m = re.match(r"^(?:Part\s+)?([A-Z]?\.?[0-9]+(?:\.[0-9]+)*)[.\s)]", text + " ")
            if m:
                nums.add(m.group(1).rstrip("."))
        cache[path] = nums
    return cache[path]


def trailing_sections(md, no, target, cache={}):
    """'§X.Y' written right after a link: '[04](04-solver.md) §3.2' or '… §3.2, §4'."""
    if md not in cache:
        cache[md] = md.read_text(encoding="utf-8").splitlines()
    line = cache[md][no - 1]
    out = []
    for m in re.finditer(r"\]\(" + re.escape(target) + r"\)((?:\s*(?:§\s*[A-Z]?\.?[0-9][0-9.]*|,|and)\s*)+)", line):
        out += SECTION.findall(m.group(1))
    return out


def main(argv):
    quiet = "-q" in argv
    errors = warnings = 0
    root = DOCS.parent
    files = sorted(DOCS.glob("*.md")) + [root / "README.md", root / "CONTRIBUTING.md", root / "zyppng" / "CONTRIBUTING.md"]
    for md in files:
        if not md.exists():
            continue
        for no, text, target in links(md):
            if re.match(r"^[a-z]+:", target) or target.startswith("mailto:"):
                continue  # external
            file_part, _, anchor = target.partition("#")
            dest = (md.parent / file_part).resolve() if file_part else md
            where = f"{md.relative_to(root)}:{no}"
            if file_part and not dest.exists():
                print(f"ERROR {where}: missing file {file_part}")
                errors += 1
                continue
            if dest.suffix != ".md":
                continue
            if anchor and anchor not in anchors_of(dest):
                print(f"ERROR {where}: missing anchor #{anchor} in {dest.name}")
                errors += 1
            for sec in SECTION.findall(text) + trailing_sections(md, no, target):
                if sec.rstrip(".") not in section_numbers(dest):
                    warnings += 1
                    if not quiet:
                        print(f"warn  {where}: '§{sec}' not found in {dest.name} ({text!r})")
    print(f"{errors} error(s), {warnings} warning(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
