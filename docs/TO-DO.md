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
- The pycc compiler must enforce all ACTIVE pep standards (https://peps.python.org/)
- use polymorphism to reduce cognitive complexity and improve maintainability, reducing the need for if/switch.

---

## Milestone 1 — Minimum End-to-End Stack (MVP)

### Goal: 
- Compile a small, typed subset of Python 3 to a native binary using LLVM, with a minimal runtime and GC scaffold. 
- Produce `.ll`, `.asm`, and the final executable. Provide GCC-like CLI for basic workflows.

### Scope:
- CLI (user-interface)
  - `-h/--help` usage output
  - `-o <file>` output selection
  - `-S` (emit assembly, no link), `-c` (object only)
  - Future placeholders: `-I`, `-L`, `-l`, `--target`, `--mcpu`
  - object-oriented design/modular code structure. clear abstraction boundaries for each step.
- Compiler (compiler core)
  - Wraps the entire compiler pipeline in a single entry point.
  - object-oriented design/modular code structure. clear abstraction boundaries for each step.
  - CLI parsing, tokenization, parsing, diagnostics, AST, IR, codegen, observability
- Observability
  - Centralized point of data collection for metrics and logs
  - Object-oriented design/modular code structure. clear abstraction boundaries for each step.
  - Metrics:
    - build time
      - per-file
      - per-function
      - per-stage 
      - per-type
      - per-builtin
      - per-intrinsic
      - per-intrinsic-call
    - AST geometry (depth, width, node count, edge count, etc.)
    - Lexer (token count, warning count, error count)
    - Parser (token count, error count, warning count)
    - AST Optimizer (pass count, pass time, starting node count, ending node count, etc.)
    - LLVM Optimizer (pass count, pass time, starting instruction count, ending instruction count, etc.)
    - JSON format for metrics.
  - Logs:
    - token log (lexer)
    - import log
    - AST log (for debugging)
    - IR log (for debugging)
    - codegen log (for debugging)
    - Structured logging (JSON format)
- Lexer
  - Abstract object-oriented design/modular code structure. clear abstraction boundaries for each step.
  - Processes input files as a file stream.
  - implements the docs/python3.ebnf grammar for lexer/tokenization.  Produces tokens as a stream.
  - Logs tokens as they are detected before pushing them to the parser's stream.
- Parser
  - Parser exists as an object-oriented class with clear interfaces and modular structure.
  - Parser consumes tokens stream from the lexer and produces a minimal AST for the docs/python3.ebnf grammar.
  - Parser supports a minimal subset: module, `def` functions, arguments with optional annotations, `return`, 
    assignments, integer/boolean/float/string literals, binary ops (`+ - * / %`), comparisons, if/else, while; and
    remaining grammar is silently ignored but logged as a `noop` (no operation).
  - Types & Semantics
    - Basic types: `int`, `bool`, `float`, `str`, `float`, `None`
    - Annotation ingestion from hints (PEP 484/585 style syntax where applicable)
    - Local inference for unannotated variables and expressions (principal types)
    - Function signatures: annotated parameters/returns required for public functions; infer inside bodies
    - Semantic checks: undefined names, return type compatibility, simple flow-sensitive type refinement where obvious
- Optimizer
  - Exists as a single class with clear interfaces and modular structure.
  - Performs AST transformations to improve code quality and performance (folding, algebraic simplification, etc.)
  - Emits statistics to allow optimization to be measured.
- Codegen
  - Exists as a single class with clear interfaces and modular structure.
  - Lower from AST to an internal to LLVM IR
  - class/struct, Functions, locals, arithmetic, comparisons, branches, returns
  - Class/struct layout should resemble C++ binary layout.
  - Functions should follow C++ binary layouts/conventions.
  - Emit `.ll` (human readable), `.bc` (binary) and `.asm` (per `-S` or default-on CMake option)
  - Link final executable when not `-S`/`-c`
- Runtime (ABI + GC scaffold)
  - object-oriented design/modular code structure. clear abstraction boundaries for each step.
  - Minimal ABI: `int main(int argc, char** argv)`
  - Pure C ABI boundary for entry helpers and minimal builtins
  - Minimal object representations for `int`, `bool`, `float`, `str`, `float`, `None`
  - GC v0: stop-the-world precise collector stub with root set management API; single-threaded, abstracted for later
    enhancements/replacement as a self-contained module/class
- Build & Tooling
  - CMake+Ninja; Makefile thin,minimal wrapper in place
  - clang-tidy enabled via `.clang-tidy`
  - clang-tidy-plugins:
    - {src,include}/clang-tidy/**/*.{cpp,h} defines custom clang-tidy plugins to enforce coding standards.
    - clang-tidy-plugins should be built by 'make configure' and installed to the bin/clang-tidy/<plugin> directory.
    - cmake should use bin/clang-tidy/<plugin> to run clang-tidy on each source file.
    - 'make lint' should call cmake to run the linters.
  - Tests: unit (lexer, parser, types, CLI), integration (compile module to IR/ASM), e2e (compile+run small program)

### Deliverables:
- clean build lint and test cycle demonstrating cmake-ninja pipeline works with parallelism features enabled.
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
- DEMO: Create demos/minimal.py which should be built by 'make demo' to build `build/demos/minimal`.

### Notes:
- CLI implemented with `-h/--help`, `-o`, `-S`, `-c`, color/config flags, metrics (`--metrics`, `--metrics-json`).
- Streaming lexer with LIFO input stack and optional token logging.
- Parser for the Milestone 1 subset (functions, returns, assignments, literals, binary/unary ops, comparisons, if/else,
  calls).
- Sema performs basic checks and annotates expression types; diagnostics include source snippets and optional color.
- Codegen emits LLVM IR; assembly/object/binary via clang; `.ll`/`.asm` controllable.
- Observability: stage timings, AST geometry, JSON metrics; timestamped logs to `--log-path`.
- Optimizer framework with visitor-based passes: constant folding, algebraic simplification, basic DCE.
- Build & test pipeline via CMake+Ninja; unit/integration/e2e tests; `make demo` builds `demos/minimal.py`.

***Status:*** Completed

---

## Milestone 2 — Type System Expansion

### Scope:
- Collections: `list[T]`, `tuple[...]`, `dict[K, V]` (read-only ops initially)
- Callable types and simple overload resolution
- Optional types (`T | None`), union types (limited), and `isinstance` checks
- Improved inference (unification with occurs check) and diagnostics

### Deliverables:
- Functions exercising lists/tuples compile and run
- Errors surface with source ranges and actionable messages (where messages specify line number and column of error)
- Python3 Demo programs in demos/**/*.py which compile successfully and demonstrate all implemented language features,
  wrapped by end-to-end tests.

### Acceptance Tests:
- 95% unit test coverage of the implemented language features with 'green' tests for ARM64 and AMD64 using linux and macOS targets.
- 95% integration test coverage of the implemented language features with 'green' tests for ARM64 and AMD64 using linux and macOS targets.
- 100% e2e test coverage of the implemented language features with at least 25% of the language implemented.
- All functions, classes, methods and types have /*** ... */ docstrings which define their name, purpose, any inputs 
  and outputs as well as a theory of operation or expected behavior.
- All tests are organized by unit, integration or e2e test category with only one test per file and all TEST(){...} macros 
  implemented consistently and preceded immediately with a /***...*/ docstring identifying name, code under test and
  expected behavior.

***Status:*** Completed

### Notes:
- Parser and lexer extended to support tuple/list/dict type identifiers; limited Optional syntax (`T | None`) parsed.
- AST includes TupleLiteral and ListLiteral with visitor-based traversal and structural annotations.
- Sema adds builtin typing for `len()` and `isinstance()`; tuple/list literals are typed; tuple returns validated.
- Codegen lowers tuple returns to `{ i32, i32 }` and implements builtins:
  - `len(<tuple|list> literal)` lowers to constant length (i32).
  - `isinstance(x, T)` lowers to constant bool for T in {int,bool,float} using parameter/var types.
- Flow refinement: `if isinstance(x, T):` refines `x` to `T` within the then-branch during sema.
- Strings: string literals are parsed and typed; `len("...")` lowers to a constant.
- Collections are read-only in this milestone: literals compile; variable storage for lists/tuples is intentionally unsupported.
- Optional/Union parsing is recognized for simple `T | None` in signatures; semantic use is deferred to M3.

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
  exercise all language features on linux/macOS and arm64/amd64 targets.
