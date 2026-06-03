#!/usr/bin/env python3
from __future__ import annotations

import argparse
import gzip
import json
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path


INCLUDED_ROOTS = {
    "source",
    "diagnostic",
    "preprocessor",
    "lexer",
    "parser",
    "semantic",
    "codegen",
    "compiler",
    "runtime",
}

EXCLUDED_PARTS = {
    "_deps",
    "test",
}


def is_project_source(repo_root: Path, path: Path) -> bool:
    try:
        relative = path.resolve().relative_to(repo_root)
    except ValueError:
        return False
    if not relative.parts:
        return False
    if relative.parts[0] not in INCLUDED_ROOTS:
        return False
    return not any(part in EXCLUDED_PARTS or part.startswith("build") for part in relative.parts)


def gcov_json_files(output_dir: Path) -> list[Path]:
    return sorted(output_dir.glob("*.gcov.json.gz"))


def run_gcov(build_dir: Path, output_dir: Path) -> None:
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True)

    gcno_files = sorted(build_dir.rglob("*.gcno"))
    if not gcno_files:
        raise SystemExit(f"no .gcno files found under {build_dir}; configure with --coverage first")

    failures: list[str] = []
    for gcno in gcno_files:
        command = [
            "gcov",
            "--json-format",
            "--preserve-paths",
            "--branch-counts",
            "-o",
            str(gcno.parent),
            str(gcno),
        ]
        completed = subprocess.run(
            command,
            cwd=output_dir,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if completed.returncode != 0:
            failures.append(f"{gcno}: {completed.stderr.strip()}")

    if failures:
        detail = "\n".join(failures[:20])
        raise SystemExit(f"gcov failed for {len(failures)} objects:\n{detail}")


def load_coverage(repo_root: Path, output_dir: Path) -> dict[tuple[Path, int], int]:
    line_counts: dict[tuple[Path, int], int] = defaultdict(int)
    for json_file in gcov_json_files(output_dir):
        with gzip.open(json_file, "rt", encoding="utf-8") as stream:
            payload = json.load(stream)
        for file_entry in payload.get("files", []):
            raw_file = Path(file_entry.get("file", ""))
            source_path = raw_file if raw_file.is_absolute() else repo_root / raw_file
            if not is_project_source(repo_root, source_path):
                continue
            relative = source_path.resolve().relative_to(repo_root)
            for line in file_entry.get("lines", []):
                count = line.get("count")
                line_number = line.get("line_number")
                if count is None or line_number is None:
                    continue
                line_counts[(relative, int(line_number))] += int(count)
    return line_counts


def summarize(line_counts: dict[tuple[Path, int], int]) -> tuple[int, int, dict[str, tuple[int, int]]]:
    by_root: dict[str, list[int]] = defaultdict(lambda: [0, 0])
    total = 0
    covered = 0
    for (path, _line), count in line_counts.items():
        total += 1
        by_root[path.parts[0]][1] += 1
        if count > 0:
            covered += 1
            by_root[path.parts[0]][0] += 1
    return covered, total, {key: (value[0], value[1]) for key, value in by_root.items()}


def print_summary(covered: int, total: int, by_root: dict[str, tuple[int, int]]) -> None:
    ratio = covered / total * 100.0 if total else 0.0
    print(f"C++ line coverage: {ratio:.2f}% ({covered}/{total})")
    for root, (root_covered, root_total) in sorted(by_root.items()):
        root_ratio = root_covered / root_total * 100.0 if root_total else 0.0
        print(f"  {root:12s} {root_ratio:6.2f}% ({root_covered}/{root_total})")


def main() -> int:
    parser = argparse.ArgumentParser(description="Collect gcov line coverage for cp C++ sources.")
    parser.add_argument("--repo-root", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--minimum", type=float)
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    build_dir = args.build_dir.resolve()
    output_dir = build_dir / "coverage" / "gcov-json"

    run_gcov(build_dir, output_dir)
    line_counts = load_coverage(repo_root, output_dir)
    covered, total, by_root = summarize(line_counts)
    print_summary(covered, total, by_root)

    if total == 0:
        print("coverage collection produced no project source lines", file=sys.stderr)
        return 2

    ratio = covered / total * 100.0
    if args.minimum is not None and ratio < args.minimum:
        print(f"C++ coverage {ratio:.2f}% is below required {args.minimum:.2f}%", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
