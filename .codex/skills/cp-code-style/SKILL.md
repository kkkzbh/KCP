---
name: cp-code-style
description: Code-writing style and validation workflow for the /home/kkkzbh/code/cp KCP compiler repo. Use before editing C++ modules, tests, CMake, or project documentation that replaces code_style.md.
---

# CP Code Style

## Overview

Use this skill before changing code in this repo. Keep the source modern, direct, and invariant-driven: do not hide broken assumptions with fallback code; put mechanical checks into scripts and structural invariants into tests.

## Workflow

1. Read nearby modules first and follow their current module, naming, and layout boundaries.
2. Make the smallest coherent change that improves the architecture; do not add compatibility aliases unless the user asks.
3. After edits, run:

```bash
python3 .codex/skills/cp-code-style/scripts/check_cp_style.py
```

Then run the relevant build/test command for the touched surface.

## C++ Style

- Use trailing return types: `auto f(...) -> T`.
- Prefer designated initializers for returned objects and temporary config values.
- Keep `type const&` ordering for const references and pointers. Do not add low-value local `const`.
- Prefer `std::string_view` for read-only string parameters; use `std::string` when ownership or lifetime extension is needed.
- Do not use `.at()`. Use `[]` when the index/key is an invariant, or a prior `find`/`contains` check when absence is expected.
- Prefer abbreviated/constrained template parameters over a separate `typename` plus `requires` clause when the constraint directly describes the parameter, such as `template<Range T>` instead of `template<typename T> requires Range<T>`.
- Prefer literal helpers such as `uz` and `sv` where available.
- Prefer brace construction for objects when that does not change overload selection. Do not mechanically rewrite constructors such as `std::string(view)`, `std::string(count, ch)`, `std::string(begin, end)`, or `std::vector(begin, end)`. Use `T{args}` for single-line braced construction and `T {` only when the matching `}` is on a later line.
- For single-field wrapper structs, prefer an explicit constructor and direct construction such as `id_type{raw}` instead of aggregate/designated initialization like `id_type{ .value = raw }`.
- Prefer `struct`. Put data members at the bottom, write only necessary `private:` sections, and use a newline before the struct left brace.
- Avoid redundant `[[nodiscard]]` on functions. If a return type itself must not be ignored, put `[[nodiscard]]` on the type.
- If a function needs an attribute, put the attribute on its own line. For exports, use `export [[nodiscard]]` on one line and the declaration on the next line.
- Do not put bare `export` on its own line. Prefer exporting the function definition directly; write a separate declaration only when a later definition must be referenced before it appears.
- Prefer range-for, `std::ranges`, and `std::views` when they keep the code clearer.
- If a `for` statement cannot remain single-line, prefer `std::ranges::for_each (` with the structured multiline call style.
- Keep each function declaration signature, function definition signature, and lambda signature on one line. Do not split formal parameter lists, trailing return types, lambda captures, lambda specifiers, or lambda return types across multiple lines; this does not apply to function calls, initializer lists, or object construction.
- In optional-like contexts, prefer contextual `bool` conversion over `.has_value()`: write `if(value)` / `if(not value)`. For temporary optional results, consider condition declarations such as `if(auto item = parse_item()) { ... }` to keep the value scoped to the branch.
- Any statement that must span multiple lines should use the structured multiline style. For multiline calls and multiline control conditions, put one space before the opening `(`. For multiline braced object construction, put one space before `{`. Closing delimiters are a hard alignment rule: a closing `)`, `}`, `);`, or `};` must align with the statement that opened that multiline construct, not with the last argument. This matches same-line-brace `if`/`for`: `if(x) {` closes with `}` under `if`; `if (` closes with `) {` under `if`; `ranges::for_each (` closes with `);` under `ranges::for_each`; `return std::pair {` closes with `};` under `return`; and `values.emplace_back (` closes with `);` under `values.emplace_back`.
- For multiline initializers that contain `=`, wrap the right-hand expression in an outer parenthesized block when the expression is not already a braced construction. Braced construction is already structured, so write `auto result = parse_result { ... };`, not `auto result = (parse_result { ... });`.
  ```cpp
  auto name = (
      values
      | std::views::take(3uz)
      | std::views::transform([&](auto value) {
          return value + 1;
      })
  );
  ```
- Prefer container `emplace_back` and `emplace` over constructing a value and then calling `push_back` or single-value `insert`; when using `emplace_back`, pass constructor arguments directly instead of writing `emplace_back(Type{...})`.
- Declaration specifier order is `auto constexpr static inline name`, for variables and functions alike. Keep `export` and attributes before the declaration, then put `auto` first in the declaration specifier sequence.

## Architecture Principles

- Make illegal states explicit at the type, API, or test boundary instead of letting them drift through production code.
- Do not use broad fallback branches to hide broken assumptions. If a state is architecturally impossible, expose the invariant violation and fix the upstream boundary.
- Prefer repairing the source of an invariant violation over patching downstream symptoms.
- Keep production code direct and focused on the valid execution path. Put internal invariant checks in tests; reserve production contracts for real external boundaries.
- For closed, unreachable production paths, use language-level unreachable markers where appropriate instead of exception-based internal control flow.
- Keep runtime and link dependencies scoped to the targets that actually need them.

## Deterministic Checks

The script at `scripts/check_cp_style.py` intentionally checks only simple, low-false-positive rules: user-defined `class`, prefix `const T&`/`const T*`, non-trailing function return types, same-line function attributes, declaration specifier order around `auto`, explicit optional `.has_value()`, banned `.at()`, obvious `pair`/`tuple`/`optional` parenthesized construction, and obvious construct-then-insert container calls such as `push_back(Type{...})` or `emplace_back(Type{...})`. Formatting and alignment rules live primarily in this skill text because they need judgment.
