SHELL := /bin/sh

CMAKE ?= cmake
CC ?= clang
CXX ?= clang++
JOBS ?=

# Timestamped build directories to keep repo root clean
LAST_TS_FILE := build/.last_run_ts
RUN_TS := $(shell mkdir -p build >/dev/null 2>&1; if [ -f $(LAST_TS_FILE) ]; then cat $(LAST_TS_FILE); else date +%Y%m%d-%H%M%S | tee $(LAST_TS_FILE); fi)
OUTDIR := build/tmp-$(RUN_TS)
OUTDIR_LINT := $(OUTDIR)-lint

.PHONY: presets configure build build-inter test clean lint demo coverage

presets:
	@$(CMAKE) -P cmake/GeneratePresets.cmake

configure:
	@echo "[configure] Configuring in $(OUTDIR)"
	@CC=$(CC) CXX=$(CXX) CMAKE_C_COMPILER=$(CC) CMAKE_CXX_COMPILER=$(CXX) $(CMAKE) -S . -B $(OUTDIR) -G Ninja

build:
	@$(CMAKE) --build $(OUTDIR) --parallel $(JOBS)

test:
	@# Ensure the main build tree exists and is configured
	@CC=$(CC) CXX=$(CXX) CMAKE_C_COMPILER=$(CC) CMAKE_CXX_COMPILER=$(CXX) $(CMAKE) -S . -B $(OUTDIR) -G Ninja
	@$(CMAKE) --build $(OUTDIR) --parallel $(JOBS)
	@# Run tests with progress, verbose streaming, and a hard 300s timeout
	@ctest --test-dir $(OUTDIR) --output-on-failure --progress -VV --timeout 300 --stop-on-failure

clean:
	@rm -rf build build-lint cmake-build-*
	@mkdir -p build

lint:
	@echo "[lint] configuring lint tree at $(OUTDIR_LINT)"
	@CC=$(CC) CXX=$(CXX) CMAKE_C_COMPILER=$(CC) CMAKE_CXX_COMPILER=$(CXX) $(CMAKE) -S . -B $(OUTDIR_LINT) -G Ninja -DPYCC_ENABLE_TIDY=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@echo "[build-inter] building clang-tidy plugins (pycc_tidy)"
	@env -u MAKEFLAGS $(CMAKE) --build $(OUTDIR_LINT) --target pycc_tidy --parallel $(JOBS) || echo "[build-inter] plugin build skipped/unavailable"
	@echo "[lint] run clang-tidy via CMake target"
	@env -u MAKEFLAGS $(CMAKE) --build $(OUTDIR_LINT) --target tidy --parallel $(JOBS)

build-inter:
	@echo "[build-inter] deprecated; use 'make lint'"

demo: build
	@$(CMAKE) -E make_directory $(OUTDIR)/demos
	@$(OUTDIR)/pycc -o $(OUTDIR)/demos/minimal demos/minimal.py
	@echo "[demo] Done: $(OUTDIR)/demos/minimal"

coverage:
	@echo "[coverage] Reconfiguring with coverage flags in $(OUTDIR)"
	@$(CMAKE) -S . -B $(OUTDIR) -G Ninja -DPYCC_COVERAGE=ON
	@$(CMAKE) --build $(OUTDIR) --parallel $(JOBS)
	@echo "[coverage] Running tests with coverage runtime"
	@LLVM_PROFILE_FILE=$(OUTDIR)/coverage-%p.profraw ctest --test-dir $(OUTDIR) --output-on-failure || true
	@# If coverage files weren't produced (e.g., tests not run), run them now
	@sh -c 'ls $(OUTDIR)/*.profraw >/dev/null 2>&1 || { \
		echo "[coverage] No .profraw found; running tests now"; \
		LLVM_PROFILE_FILE=$(OUTDIR)/coverage-%p.profraw ctest --test-dir $(OUTDIR) --output-on-failure || true; \
	}'
	@echo "[coverage] Generating report by phase (>=95% required)"
	@PYCC_BUILD_DIR=$(OUTDIR) PYCC_COVERAGE_MIN=95 python3 tools/coverage.py || { rc=$$?; if [ $$rc -eq 1 ]; then echo "[coverage] skipped (requires llvm-cov/llvm-profdata)"; else exit $$rc; fi; }
