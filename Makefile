SHELL := /bin/sh

CMAKE ?= cmake
CC ?= clang
CXX ?= clang++
JOBS ?=

# Unified out-of-source build directory
OUTDIR := build
OUTDIR_LINT := $(OUTDIR)/lint

.PHONY: presets configure build build-inter test clean lint demo coverage cover

presets:
	@$(CMAKE) -P cmake/GeneratePresets.cmake

configure:
	@echo "[configure] Configuring in $(OUTDIR)"
	@# Remove any stray top-level Testing directory from prior runs
	@$(CMAKE) -E rm -r Testing || true
	@CC=$(CC) CXX=$(CXX) CMAKE_C_COMPILER=$(CC) CMAKE_CXX_COMPILER=$(CXX) $(CMAKE) -S . -B $(OUTDIR) -G Ninja

build:
	@$(CMAKE) --build $(OUTDIR) --parallel $(JOBS)

test:
	@# Ensure the main build tree exists and is configured
	@CC=$(CC) CXX=$(CXX) CMAKE_C_COMPILER=$(CC) CMAKE_CXX_COMPILER=$(CXX) $(CMAKE) -S . -B $(OUTDIR) -G Ninja
	@$(CMAKE) --build $(OUTDIR) --parallel $(JOBS)
	@# Run all tests with progress and verbose streaming
	@ctest --test-dir $(OUTDIR) --output-on-failure --progress -VV -j $(JOBS)

clean:
	@echo "[clean] Removing build trees and cache dirs"
	@# Use CMake's cross-platform removal to avoid rm races
	@$(CMAKE) -E rm -r $(OUTDIR) || true
	@# Remove any IDE-generated cmake build trees if present
	@sh -c 'set -eu; for d in cmake-build-*; do [ -e "$$d" ] && $(CMAKE) -E rm -r "$$d" || true; done'
	@# Remove any stray Testing/ directory in repo root (tests should write under build/Testing)
	@$(CMAKE) -E rm -r Testing || true

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
	@echo "[coverage] Running all tests with coverage runtime"
	@LLVM_PROFILE_FILE=$(OUTDIR)/coverage-%p.profraw ctest --test-dir $(OUTDIR) --output-on-failure || true
	@# If coverage files weren't produced (e.g., tests not run), run them now
	@sh -c 'ls $(OUTDIR)/*.profraw >/dev/null 2>&1 || { \
		echo "[coverage] No .profraw found; running tests now"; \
		LLVM_PROFILE_FILE=$(OUTDIR)/coverage-%p.profraw ctest --test-dir $(OUTDIR) --output-on-failure || true; \
	}'
	@echo "[coverage] Generating report (no gating)"
	@PYCC_BUILD_DIR=$(OUTDIR) PYCC_COVERAGE_MIN=0 python3 tools/coverage.py || { rc=$$?; if [ $$rc -eq 1 ]; then echo "[coverage] skipped (requires llvm-cov/llvm-profdata)"; else exit $$rc; fi; }

.PHONY: runtime-cover
runtime-cover:
	@echo "[runtime-cover] Reconfiguring with coverage flags in $(OUTDIR)"
	@$(CMAKE) -S . -B $(OUTDIR) -G Ninja -DPYCC_COVERAGE=ON
	@$(CMAKE) --build $(OUTDIR) --parallel $(JOBS)
	@echo "[runtime-cover] Running runtime-only tests with coverage"
	@$(CMAKE) -E rm -f $(OUTDIR)/*.profraw || true
	@LLVM_PROFILE_FILE=$(OUTDIR)/coverage-%p.profraw $(OUTDIR)/test_runtime_only --gtest_color=yes --gtest_filter=Runtime* || true
	@echo "[runtime-cover] Merging + reporting for runtime phase (min 96%)"
	@PYCC_BUILD_DIR=$(OUTDIR) PYCC_COVERAGE_MIN=100 PYCC_COVERAGE_PHASES=runtime python3 tools/coverage.py || { rc=$$?; if [ $$rc -eq 1 ]; then echo "[runtime-cover] skipped (requires llvm-cov/llvm-profdata)"; else exit $$rc; fi; }

# Short alias per user request
cover: coverage
.PHONY: sema-cover
sema-cover:
	@echo "[sema-cover] Configuring with coverage flags in $(OUTDIR)"
	@$(CMAKE) -S . -B $(OUTDIR) -G Ninja -DPYCC_COVERAGE=ON
	@$(CMAKE) --build $(OUTDIR) --parallel $(JOBS)
	@echo "[sema-cover] Running all tests with coverage"
	@$(CMAKE) -E rm -f $(OUTDIR)/*.profraw || true
	@LLVM_PROFILE_FILE=$(OUTDIR)/coverage-%p.profraw $(OUTDIR)/test_unit --gtest_color=yes || true
	@LLVM_PROFILE_FILE=$(OUTDIR)/coverage-%p.profraw $(OUTDIR)/test_integration --gtest_color=yes || true
	@LLVM_PROFILE_FILE=$(OUTDIR)/coverage-%p.profraw $(OUTDIR)/test_e2e --gtest_color=yes || true
	@echo "[sema-cover] Enforcing sema phase at 100%"
	@PYCC_BUILD_DIR=$(OUTDIR) PYCC_COVERAGE_MIN=100 PYCC_COVERAGE_PHASES=sema python3 tools/coverage.py || { rc=$$?; if [ $$rc -eq 1 ]; then echo "[sema-cover] skipped (requires llvm-cov/llvm-profdata)"; else exit $$rc; fi; }
