# cp

`cp` is a compiler project for the cp programming language. The repository contains the language design, frontend, semantic analysis, IR and LLVM lowering, runtime support, standard library, examples, documentation, and CLion integration.

![cp in CLion](docs/assets/readme/clion-demo.png)

## Overview

The compiler is organized as a staged native-code toolchain:

- `source`, `preprocessor`, and `lexer` preserve source locations and produce diagnostics-friendly token streams.
- `parser` builds the cp syntax tree from modules, declarations, statements, expressions, and types.
- `semantic` resolves names, checks types, instantiates generics, validates concepts, and lowers language rules into typed compiler state.
- `codegen` translates checked programs through IR and LLVM.
- `runtime` defines the ABI surface used by generated programs.
- `std` provides the first standard library modules written in cp.

The project also includes a documentation site, runnable examples, regression tests, and an IntelliJ/CLion language plugin.

## Language

cp is a statically typed systems language with explicit modules, value-oriented aggregates, generic programming, and predictable low-level interop.

Current language areas include:

- modules, imports, exports, and name visibility
- structs, impl blocks, constructors, destructors, member functions, and UFCS
- strong integer enums, variants, payload cases, and `match`
- references, pointers, arrays, tuples, ownership, move, and `like&`
- lambdas, function values, captures, and closure checks
- generic functions, generic types, parameter packs, and `template for`
- concepts, associated types, default implementations, and constrained APIs
- operator overloading, explicit casts, opaque aliases, and `extern "C"`

Example:

```cp
import std.core.option;

variant event {
    quit;
    key(char);
    resize(i32, i32);
}

score(value: event) -> i32
{
    return match value {
        .resize(width, height) => width + height,
        .key(code) => 1,
        .quit => 0,
    };
}

main() -> i32
{
    let some = optional<i32>::some(20);
    let none = optional<i32>::none;
    return some.value_or(0) + none.value_or(10) + score(event::resize(5, 7));
}
```

More examples are available under `design/examples/`.

## Standard Library

The standard library is implemented as ordinary cp modules. It currently includes:

- `std.core`: `optional`, `expected`, iterator protocols
- `std.memory`: raw buffers and spans
- `std.collections`: `vector`, ordered `map`, ordered `set`
- `std.text`: `str` and owning `string`
- `std.ranges`: sources, lazy adapters, and terminals
- `std.meta`: type queries and callable concepts
- `std.compare`: comparison categories and ordering objects
- `std.algorithm`: sorting algorithms
- `std.io`: formatting and output
- `std.fs`: synchronous file IO

## Repository Layout

| Path | Contents |
| --- | --- |
| `compiler/` | command-line compiler entry points |
| `source/` | source text, spans, and location utilities |
| `preprocessor/` | source normalization before lexing |
| `lexer/` | tokenization and lexical diagnostics |
| `parser/` | parser, syntax tree, and parse diagnostics |
| `semantic/` | semantic analysis, types, generics, and language rules |
| `codegen/` | IR and LLVM code generation |
| `runtime/` | runtime ABI documentation and support |
| `std/` | cp standard library source |
| `design/` | language documentation and examples |
| `lab/` | focused compiler-construction pipelines and sample grammars |
| `clion-plugin/` | CLion plugin for cp language support |
| `test/` | compiler, library, parser, lexer, and integration tests |

## Building

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the deterministic style checks:

```bash
python3 .codex/skills/cp-code-style/scripts/check_cp_style.py
```

## Documentation

The language documentation is maintained as a VitePress site:

```bash
cd design
npm run dev
```

Useful entry points:

- [Language guide](design/docs/index.md)
- [Examples](design/docs/examples.md)
- [Standard library notes](std/README.md)
- [Runtime ABI](runtime/abi.md)
