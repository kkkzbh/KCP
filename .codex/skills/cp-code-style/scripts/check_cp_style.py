#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


SKIP_DIRS = {
    ".cache",
    ".codex",
    ".git",
    ".idea",
    ".vscode",
    "build",
    "build-clion",
    "cmake-build-debug",
    "cmake-build-release",
}

CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".cppm", ".ixx"}


def iter_cpp_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if any(part in SKIP_DIRS for part in path.relative_to(root).parts):
            continue
        if path.is_file() and path.suffix in CPP_SUFFIXES:
            files.append(path)
    return files


def add_issue(issues: list[str], path: Path, line: int, message: str) -> None:
    issues.append(f"{path}:{line}: {message}")


def check_user_class_keyword(path: Path, lines: list[str], issues: list[str]) -> None:
    pattern = re.compile(r"^\s*(?:export\s+)?class\s+[A-Za-z_]\w*")
    for number, line in enumerate(lines, 1):
        if pattern.search(line):
            add_issue(issues, path, number, "prefer struct; use private: only when needed")


def check_prefix_const_references(path: Path, lines: list[str], issues: list[str]) -> None:
    pattern = re.compile(r"\bconst\s+(?:[A-Za-z_]\w*::)*[A-Za-z_]\w*(?:<[^;\n(){}]*>)?\s*[*&]")
    for number, line in enumerate(lines, 1):
        if pattern.search(line):
            add_issue(issues, path, number, "write const references/pointers as type const& or type const*")


def check_trailing_return_type(path: Path, lines: list[str], issues: list[str]) -> None:
    pattern = re.compile(
        r"^\s*"
        r"(?:export\s+)?"
        r"(?:inline\s+|constexpr\s+|static\s+|friend\s+)*"
        r"(?P<return>[A-Za-z_:][A-Za-z0-9_:<>,\s*&]*?)\s+"
        r"(?P<name>[A-Za-z_]\w*)\s*"
        r"\([^;{}]*\)\s*"
        r"(?:const\s*)?"
        r"(?:noexcept\s*)?"
        r"(?:[;{]|=)"
    )
    for number, line in enumerate(lines, 1):
        stripped = line.strip()
        if not stripped or stripped.startswith(("//", "#")):
            continue
        if stripped.startswith(("return ", "if", "for", "while", "switch", "catch", "requires", "and ", "or ")):
            continue
        if any(operator in stripped.split("(", 1)[0] for operator in {"<<", ">>", "==", "!=", "<=", ">="}):
            continue
        match = pattern.match(line)
        if match is None:
            continue
        if match.group("return").strip() != "auto":
            add_issue(issues, path, number, "use trailing return type: auto f(...) -> T")


def check_function_attribute_line(path: Path, lines: list[str], issues: list[str]) -> None:
    pattern = re.compile(r"\[\[[^\]]+\]\].*\bauto\b.*\(")
    for number, line in enumerate(lines, 1):
        if pattern.search(line):
            add_issue(issues, path, number, "put function attributes on their own line")


def check_auto_specifier_order(path: Path, lines: list[str], issues: list[str]) -> None:
    before_auto_pattern = re.compile(r"\b(?:constexpr|static|inline)\s+(?:constexpr\s+|static\s+|inline\s+)*auto\b")
    after_auto_pattern = re.compile(r"\bauto\b(?P<tail>(?:\s+(?:constexpr|static|inline)\b)+)")
    order = {"constexpr": 0, "static": 1, "inline": 2}
    for number, line in enumerate(lines, 1):
        if before_auto_pattern.search(line):
            add_issue(issues, path, number, "put auto before constexpr/static/inline in declarations")
            continue

        match = after_auto_pattern.search(line)
        if match is None:
            continue

        specifiers = match.group("tail").split()
        if specifiers != sorted(specifiers, key=order.__getitem__):
            add_issue(issues, path, number, "write declaration specifiers as auto constexpr static inline")


def check_bare_export_line(path: Path, lines: list[str], issues: list[str]) -> None:
    for number, line in enumerate(lines, 1):
        if line.strip() == "export":
            add_issue(issues, path, number, "do not put bare export on its own line")


def check_optional_has_value(path: Path, lines: list[str], issues: list[str]) -> None:
    pattern = re.compile(r"\.has_value\s*\(")
    for number, line in enumerate(lines, 1):
        if pattern.search(line):
            add_issue(issues, path, number, "prefer contextual bool conversion for optional-like values")


def check_construct_then_insert(path: Path, lines: list[str], issues: list[str]) -> None:
    push_pattern = re.compile(r"\.push_back\s*\(\s*(?:[A-Za-z_:][A-Za-z0-9_:<>]*\s*)?\{")
    emplace_pattern = re.compile(r"\.emplace_back\s*\(\s*(?:[A-Za-z_:][A-Za-z0-9_:<>]*\s*)?\{")
    insert_pattern = re.compile(r"\.insert\s*\(\s*(?:[A-Za-z_:][A-Za-z0-9_:<>]*\s*)?\{")
    for number, line in enumerate(lines, 1):
        if push_pattern.search(line):
            add_issue(issues, path, number, "prefer emplace_back over push_back with a freshly constructed value")
        if emplace_pattern.search(line):
            add_issue(issues, path, number, "pass constructor arguments directly to emplace_back; do not wrap a freshly constructed value")
        if insert_pattern.search(line):
            add_issue(issues, path, number, "prefer emplace over single-value insert with a freshly constructed value")


def check_parenthesized_object_construction(path: Path, lines: list[str], issues: list[str]) -> None:
    pattern = re.compile(r"\bstd::(?:pair|tuple|optional)\s*(?:<[^;\n(){}]*>)?\s*\(")
    for number, line in enumerate(lines, 1):
        match = pattern.search(line)
        if match is not None and (match.start() == 0 or line[match.start() - 1] != "<"):
            add_issue(issues, path, number, "prefer brace construction for standard objects: T{args}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Check deterministic cp repo C++ style rules.")
    parser.add_argument("paths", nargs="*", type=Path, default=[Path(".")])
    args = parser.parse_args()

    root = Path.cwd().resolve()
    selected: list[Path] = []
    for raw in args.paths:
        path = (root / raw).resolve() if not raw.is_absolute() else raw.resolve()
        if path.is_dir():
            selected.extend(iter_cpp_files(path))
        elif path.is_file() and path.suffix in CPP_SUFFIXES:
            selected.append(path)

    issues: list[str] = []
    for absolute in sorted(set(selected)):
        try:
            relative = absolute.relative_to(root)
        except ValueError:
            relative = absolute
        if any(part in SKIP_DIRS for part in relative.parts):
            continue

        lines = absolute.read_text(encoding="utf-8").splitlines()
        check_user_class_keyword(relative, lines, issues)
        check_prefix_const_references(relative, lines, issues)
        check_trailing_return_type(relative, lines, issues)
        check_function_attribute_line(relative, lines, issues)
        check_auto_specifier_order(relative, lines, issues)
        check_bare_export_line(relative, lines, issues)
        check_optional_has_value(relative, lines, issues)
        check_construct_then_insert(relative, lines, issues)
        check_parenthesized_object_construction(relative, lines, issues)

    if issues:
        print("\n".join(issues))
        return 1

    print("cp style checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
