# pycc — A Native Python 3 Compiler (C++23, LLVM)

The `pycc` compiler  is a ahead-of-time (AOT) compiler for (a statically analyzable subset of) Python 3. It is 
implemented in C++23 and targets LLVM. The compiler enforces Python 3 type hints and performs semantic type inference 
where annotations are omitted, producing optimized native binaries without any dependency on CPython.

Key design points:

- Strong type-hint enforcement with inference when hints are absent
- No CPython embedding; a small, pure C ABI runtime with a GC
- Emits LLVM IR (`.ll`), target assembly (`.asm`), and final binaries
- Built with CMake + Ninja; simple Makefile wrapper for common tasks
- Linted with `clang-tidy` (configured via a project `.clang-tidy`)
- Header/implementation split: declarations in `.h`, definitions in `.cpp`
- One function or method per `.cpp` file
- Required docstrings for every declaration and definition

> Status: This repository documents the intended design and workflow. Some features described below may be in active 
> development.


## Goals

- Compile a maintainable, types-first subset of Python 3 to fast native code.
- Provide predictable performance and a clear FFI surface via a pure C ABI.
- Keep the runtime minimal, portable, and independent of CPython internals.
- Make development ergonomic with modern tools (CMake, Ninja, clang-tidy).


## Type System & Semantics

- Strongly enforced annotations: Standard Python 3 type hints (PEP 484/585/604/etc.) are treated as constraints, not 
  optional metadata. Violations are compile-time errors.
- Inference when missing: When annotations are absent, the semantic analyzer infers principal types from usage, 
  literals, control flow, and function call sites. Ambiguity results in a compile-time error with actionable 
  diagnostics.
- Gradual typing compatibility: Codebases can mix annotated and unannotated regions; the compiler infers types at 
  boundaries and reports unsafe interactions.
- Generics and protocols: The typing surface aims to follow Python’s standard typing module. Specialization and 
  monomorphization strategies are used where profitable.
- Runtime types: The runtime includes efficient type representations to support isinstance checks, pattern matching,
  and generic instantiation without CPython.


## Runtime (Pure ABI + GC)

- No CPython: pycc does not embed or depend on CPython. Execution uses a compact runtime implemented in C++ with a 
  stable C ABI surface for FFI.
- C ABI: Foreign calls use a pure C ABI boundary (`extern "C"`)—easy to consume from C/C++/Rust/Go and to embed into
  other systems.
- Garbage collector: The runtime includes a GC (design is precise and stop‑the‑world by default; further tuning and 
  generations may be introduced as the project evolves).
- Standard library surface: A minimal set of built-ins is provided in the runtime; broader stdlib coverage is planned
  as compiled modules.


## Outputs

pycc can emit multiple artifacts for inspection and downstream tooling:

- LLVM IR: Human-readable `.ll` files
- Assembly: Target `.asm` files
- Binary: A native executable or library for the target triple

Planned CLI example:

```bash
pycc my_module.py --emit=ll,asm,bin --out-dir build/py
# Control target triple / CPU features
pycc my_module.py --target=x86_64-unknown-linux-gnu --mcpu=native
```


## Architecture

- OOP design: Compiler is organized into modular components (driver, frontend,
  types/sema, IR, backend, runtime). Each component exposes a clear interface
  in headers and hides implementation details in sources.
- RAII everywhere: Resource management uses RAII (scoped handles, timers,
  and cleanup through destructors). Metrics timing uses scoped RAII timers.
- Testability: Components are decoupled and unit-testable. Integration and e2e
  tests exercise end-to-end flows.

- AST polymorphism: The AST now supports polymorphic traversal via
  `ast::Node::accept(ast::VisitorBase&)` and concrete visitors. This replaces
  ad hoc `if`/`switch` branches across Sema, Codegen, and geometry with a
  single central dispatch and specialized visitors, reducing cognitive
  complexity and making extensions safer.


## CLI Usage

Usage:

```text
pycc [options] file...

Options:
  -h, --help           Print this help and exit
  -o <file>            Place the output into <file> (default: a.out)
  -S                   Compile only; generate assembly (do not link)
  -c                   Compile and assemble (object file); do not link
  --metrics            Print compilation metrics summary
  --                    End of options
```

Notes:

- By default, builds enable emission of LLVM IR and ASM; disable via CMake if desired.
- Follows GCC/G++-style switches for common flows (`-o`, `-S`, `-c`).

Examples:

```bash
# Build native binary from a single module
pycc -o app main.py

# Generate assembly only (no link)
pycc -S -o main.s main.py

# Compile to object file (no link)
pycc -c -o main.o main.py

# Link multiple modules
pycc -o app module1.py module2.py

# Inputs that start with '-' after end-of-options marker
pycc -- -strange-name.py

# Show help
pycc --help
```

## Metrics

- Flag: `--metrics` prints a human-friendly summary of phase durations, AST geometry
  (node count and maximum depth), and a list of optimization or transformation
  events performed during compilation.
- Flag: `--metrics-json` prints the same core metrics as structured JSON
  for machine consumption and logging systems.
- Intended use: performance profiling and visibility into the compiler pipeline.

Example output:

```text
== Metrics ==
  ReadFile: 0.019 ms
  Parse: 0.011 ms
  EmitIR: 0.004 ms
  EmitASM: 16.628 ms
  Link: 32.969 ms
  AST: nodes=4, max_depth=4
  Optimizations (1):
    - LoweredConstantReturn(main)
```



## Build System

- Toolchain: C++23, Clang/LLVM, CMake (>= 3.25), Ninja
- Wrapper: A Makefile provides convenience targets for common workflows
- Lint: `clang-tidy` configured via `.clang-tidy` at repository root

### Dependencies

Install a recent LLVM/Clang, CMake, and Ninja. Examples:

- macOS (Homebrew):
  ```bash
  brew install llvm cmake ninja
  # Optional dev tools
  brew install clang-format clang-tidy
  # Prefer Homebrew LLVM on PATH
  export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
  export LDFLAGS="-L/opt/homebrew/opt/llvm/lib"
  export CPPFLAGS="-I/opt/homebrew/opt/llvm/include"
  ```
- Ubuntu/Debian:
  ```bash
  sudo apt-get update
  sudo apt-get install -y cmake ninja-build clang llvm clang-tidy
  # For specific versions (example): clang-17, llvm-17, clang-tidy-17
  ```

### Configure & Build

Using the Makefile wrapper (recommended):

```bash
# One-time configure (Debug by default)
make configure
# Build all targets
make build
# Clean build tree
make clean
```

Equivalent raw CMake + Ninja commands:

```bash
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

To enable clang-tidy during configuration:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_CLANG_TIDY="clang-tidy;-p=build"
```

### Emitting LLVM IR and Assembly

Emission can be driven by either CLI flags (preferred) or CMake options.
The default build enables both emissions; disable explicitly if desired.

- `-DPYCC_EMIT_LLVM=OFF` – disable writing `.ll` next to build artifacts
- `-DPYCC_EMIT_ASM=OFF` – disable writing `.asm` next to build artifacts

Example (disable both):

```bash
cmake -S . -B build -G Ninja -DPYCC_EMIT_LLVM=OFF -DPYCC_EMIT_ASM=OFF
cmake --build build -j
```


## Repository Layout (Planned)

```
include/pycc/           # Public headers (API + runtime ABI)
src/                    # Implementation (.cpp; one entity per file)
  driver/               # CLI and top-level orchestration
  frontend/             # Lexer, parser, AST
  types/                # Type system, inference, constraints
  sema/                 # Semantic analysis and checks
  ir/                   # Lowering to pycc IR and LLVM
  backend/              # LLVM codegen and target-specifics
  runtime/              # GC, allocator, builtins, ABI shims
  support/              # Utilities, diagnostics, logging
runtime/                # Standalone runtime sources (when split)
cmake/                  # CMake modules and toolchain helpers
.clang-tidy             # Linter configuration
CMakeLists.txt          # Root build definition
Makefile                # Convenience wrapper for CMake/Ninja
```


## Coding Standards

- Headers vs. sources: All declarations live in `.h` files; all definitions live in `.cpp` files. Avoid inline 
  definitions except for tiny, truly trivial templates.
- One entity per file: Implement exactly one function/method/class per `.cpp` to keep compilation units small and 
  diffs precise.
- Namespaces: Use `pycc::...` namespaces mirroring the directory layout.
- Error handling: Prefer explicit error objects or `llvm::Expected<T>` patterns; avoid exceptions for control flow.
- Docs required: Every declaration and definition must include a structured docstring using the `/*** ... */` form 
  below.

Docstring template:

```c++
/***
 * Name: pycc::sema::InferTypes
 * Purpose: Infer principal types for unannotated declarations and expressions.
 * Inputs:
 *   - ast: const Ast& — The fully parsed AST of the compilation unit.
 *   - env: TypeEnv& — The mutable type environment and constraints store.
 * Outputs:
 *   - Result<TypeSummary> — Success with a summary of inferred types or rich diagnostics on failure.
 * Theory of Operation:
 *   Runs constraint generation over the AST, solves via unification with occurs checks,
 *   then generalizes monomorphic bindings and records principal types. Conflicts
 *   produce diagnostics pointing to source spans and incompatible constraints.
 */
```


## Linting and Formatting

- clang-tidy: The project uses `clang-tidy` with a repository-level `.clang-tidy`. Run it via:
  ```bash
  clang-tidy -p build $(git ls-files '*.cpp' '*.h')
  ```
  Or configure through CMake: `-DCMAKE_CXX_CLANG_TIDY=clang-tidy`.
- clang-format: While not enforced, `clang-format` is recommended for consistency.


## Testing

- Framework: GoogleTest via CTest. Enabled by default (`BUILD_TESTING=ON`).
- Segmentation: All tests must be organized by project and category:
  - `test/pycc/unit/`
  - `test/pycc/integration/`
  - `test/pycc/e2e/`
- Running: `ctest --test-dir build --output-on-failure`
- Adding tests: Drop `*.cpp` files into the appropriate category; CMake will
  build separate executables per category and auto-discover tests.


## Contributing

- Discuss significant changes via issues before large PRs.
- Keep commits small and focused; include rationale in commit messages.
- Follow the coding standards above. Ensure `clang-tidy` passes.
- New code must include `/*** ... */` docstrings for all entities.


## Roadmap (High-Level)

- Frontend: Lexer, parser, AST, source locations, diagnostics
- Types: Annotation ingestion, inference, generics, protocols
- Sema: Name resolution, overload resolution, flow-sensitive typing
- IR: pycc IR, lowering to LLVM, optimization passes
- Backend: Emission of `.ll`, `.asm`, binaries across common targets
- Runtime: GC, allocator, builtins, C ABI, FFI helpers
- Tooling: CLI flags, Makefile wrapper, `.clang-tidy` policy, CTest


## License

MIT License — see `LICENSE.txt` for details.

---

Questions or suggestions? Open an issue and share your use case.
