# pycc Roadmap and Milestones

This document tracks the near-term plan for pycc. Milestones are scoped to deliver independently useful capabilities, 
with clear acceptance criteria and test coverage. The first milestone focuses on delivering the minimum end-to-end 
stack.

## General Engineering Practices

- Target 100% Python3 3.13+ compatibility
- Headers declare; sources define. One entity per `.cpp`.
- Every declaration and definition has `/*** ... */` docstrings (name, purpose, inputs, outputs, theory of operation).
- Lint with `clang-tidy`; keep warnings at zero.
- Tests are segmented by project and category: `test/pycc/{unit,integration,e2e}/`.
- Prefer simple, correct implementations over premature optimization; add benchmarks before optimizing.
- minimum 80% test coverage (unit + integration).
- Demonstration programs are in `demos/<project>` (built as `build/demos/<project>`)
- The project must use OOP design patterns and be extensible.
- The C++ compiler code must adhere to RAII principles.

---

## Milestone 1 — Minimum End-to-End Stack (MVP)

### Goal: 
- Compile a small, typed subset of Python 3 to a native binary using LLVM, with a minimal runtime and GC scaffold. 
- Produce `.ll`, `.asm`, and the final executable. Provide GCC-like CLI for basic workflows.

### Scope:
- CLI
  - `-h/--help` usage output
  - `-o <file>` output selection
  - `-S` (emit assembly, no link), `-c` (object only)
  - Future placeholders: `-I`, `-L`, `-l`, `--target`, `--mcpu`
- Frontend
  - Lexer and parser for a minimal subset: module, `def` functions, arguments with optional annotations, `return`, 
    assignments, integer/boolean/float/string literals, binary ops (`+ - * / %`), comparisons, if/else, while
  - AST with source locations and diagnostics hooks
- Types & Semantics
  - Basic types: `int`, `bool`, `float`, `str`
  - Annotation ingestion from hints (PEP 484/585 style syntax where applicable)
  - Local inference for unannotated variables and expressions (principal types)
  - Function signatures: annotated parameters/returns required for public functions; infer inside bodies
  - Semantic checks: undefined names, return type compatibility, simple flow-sensitive type refinement where obvious
- IR & Codegen
  - Lower from AST to an internal IR then to LLVM IR
  - Functions, locals, arithmetic, comparisons, branches, returns
  - Emit `.ll` (human readable) and `.asm` (per `-S` or default-on CMake option)
  - Link final executable when not `-S`/`-c`
- Runtime (ABI + GC scaffold)
  - Pure C ABI boundary for entry helpers and minimal builtins
  - Minimal object representations for `int`, `bool`, `float`, `str`
  - GC v0: stop-the-world precise collector stub with root set management API; single-threaded
- Build & Tooling
  - CMake+Ninja; Makefile wrapper in place
  - clang-tidy enabled via `.clang-tidy`
  - Tests: unit (lexer, parser, types, CLI), integration (compile module to IR/ASM), e2e (compile+run small program)

### Deliverables:
- Compiles a simple example and runs:
  ```python
  # main.py
  def add(a: int, b: int) -> int:
      return a + b
  
  def main() -> int:
      x = add(2, 3)
      return x
  ```
  Build: `pycc -o app main.py` -> exit status 0; running `./app` returns 5 (or prints 5 and exits 0, as defined by 
  runtime contract)
- Emits `main.ll` and `main.asm` when enabled (default is ON via CMake options)
- GCC-like CLI works (`-h`, `-o`, `-S`, `-c`)

### Acceptance Tests:
- Unit: CLI parsing; tokenization/parsing of primitives; type inference of locals; annotated function checks
- Integration: `main.py` lowers to expected LLVM patterns; `.ll` and `.asm` are produced
- E2E: Build and execute example, verify observable result (exit code or stdout)
- DEMO: Create demos/minimal.py which should be build by 'make demo' to build build/demos/minimal.

***Status:*** Completed (MVP)
Notes:
- pycc compiles a minimal Python program with `def main() -> int: return <int>`
  to LLVM IR, assembly, and a native binary using system `clang`.
- CLI implements `-h/--help`, `-o`, `-S`, and `-c`.
- Current parser is intentionally limited to a constant integer `return` in `main()`;
  upcoming milestones will expand syntax, typing, and semantics.

---

## Milestone 2 — Type System Expansion

### Scope:
- Collections: `list[T]`, `tuple[...]`, `dict[K, V]` (read-only ops initially)
- Callable types and simple overload resolution
- Optional types (`T | None`), union types (limited), and `isinstance` checks
- Improved inference (unification with occurs check) and diagnostics

### Deliverables:
- Functions exercising lists/tuples compile and run
- Errors surface with source ranges and actionable messages

***Status:*** Planned

---

## Milestone 3 — Runtime v1 + GC v1

### Scope:
- Precise mark-sweep collector with root scanning and conservative stack scanning
- Allocation APIs for boxed values and strings; string interop helpers
- Error/exception model decision (initially, result/diagnostic objects; exceptions later)

### Deliverables:
- Heap stress tests (unit + e2e) without leaks or crashes
- Tunable GC thresholds and basic telemetry counters

***Status:*** Planned


## Milestone 4 — IR and Optimization Passes

### Scope:
- pycc IR validation and canonicalization passes
- LLVM optimization pipeline tuning for common Python idioms
- Dead code elimination, inlining heuristics, const folding, strength reduction

### Deliverables:
- Benchmarks show measurable improvements over unoptimized emission

***Status:*** Planned


## Milestone 5 — CLI and Tooling Growth

### Scope:
- Add `-I`, `-L`, `-l` support for includes and libraries
- `--target`, `--mcpu`, `-O[0-3]` flags
- Better diagnostics formatting (colors, source excerpts)
- Rich `--emit=` modes mirroring `clang` style

### Deliverables:
- Cross-compilation works for at least one alt target triple

***Status:*** Planned


## Milestone 6 — Modules and Stdlib Surface

### Scope:
- Multi-file module compilation and linking
- A small curated subset of Python stdlib as compiled modules
- FFI shims for C ABI

### Deliverables:
- Sample programs using multiple modules and selected stdlib components

***Status:*** Planned


## Milestone 7 — Packaging, CI, and Docs

### Scope:
- CI workflows (build, lint, test, release artifacts)
- Developer docs: contribution guide, design notes, runtime/GC docs

### Deliverables:
- Reproducible builds and published binaries for major platforms

***Status:*** Planned

## Milestone 8 - Full EBNF Grammar Implementation
### Scope:
- Using `docs/python3.ebnf` as the grammar, ensure full implementation of the language built-ins.
- Updated developer docs for the Python3 compiler
- 95% test coverage of the project with demos/<example>/*.py demonstration projects for full e2e testing.

### Deliverables:
- Example programs covering the language features (in demos/<example>/*.py)

### Acceptance Tests:
- Unit/Integration: 95% test coverage of the project with 'green' tests for ARM64 and AMD64 using linux and macOS 
  targets.
- E2E: All Example projects build and run successfully with verified input/output along happy and sad paths which
  exercise all language features on linux/macos and arm64/amd64 targets.
