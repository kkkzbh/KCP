#!/usr/bin/env python3
from __future__ import annotations

import argparse
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


def counter_value(element: ET.Element, counter_type: str) -> tuple[int, int]:
    missed = 0
    covered = 0
    for counter in element.findall("counter"):
        if counter.get("type") != counter_type:
            continue
        missed += int(counter.get("missed", "0"))
        covered += int(counter.get("covered", "0"))
    return covered, covered + missed


def print_summary(report: Path, minimum: float | None) -> int:
    root = ET.parse(report).getroot()
    line_covered, line_total = counter_value(root, "LINE")
    instruction_covered, instruction_total = counter_value(root, "INSTRUCTION")

    line_ratio = line_covered / line_total * 100.0 if line_total else 0.0
    instruction_ratio = instruction_covered / instruction_total * 100.0 if instruction_total else 0.0
    print(f"CLion plugin line coverage: {line_ratio:.2f}% ({line_covered}/{line_total})")
    print(f"CLion plugin instruction coverage: {instruction_ratio:.2f}% ({instruction_covered}/{instruction_total})")

    packages: list[tuple[str, int, int]] = []
    for package in root.findall("package"):
        covered, total = counter_value(package, "LINE")
        if total != 0:
            packages.append((package.get("name", ""), covered, total))
    for name, covered, total in sorted(packages):
        ratio = covered / total * 100.0
        print(f"  {name.replace('/', '.'):40s} {ratio:6.2f}% ({covered}/{total})")

    if line_total == 0:
        print("JaCoCo report contains no line coverage counters", file=sys.stderr)
        return 2
    if minimum is not None and line_ratio < minimum:
        print(f"CLion plugin line coverage {line_ratio:.2f}% is below required {minimum:.2f}%", file=sys.stderr)
        return 2
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Summarize JaCoCo coverage for the KCP CLion plugin.")
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--minimum", type=float)
    args = parser.parse_args()

    report = args.report.resolve()
    if not report.is_file():
        print(f"JaCoCo report not found: {report}", file=sys.stderr)
        return 2
    return print_summary(report, args.minimum)


if __name__ == "__main__":
    raise SystemExit(main())
