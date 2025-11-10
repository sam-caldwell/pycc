SHELL := /bin/sh

# Thin wrappers that defer to CMake presets
CMAKE ?= cmake
CC ?= clang
CXX ?= clang++
JOBS ?=

.PHONY: presets configure build test clean lint demo

presets:
	@$(CMAKE) -P cmake/GeneratePresets.cmake

configure: presets
	@CC=$(CC) CXX=$(CXX) CMAKE_C_COMPILER=$(CC) CMAKE_CXX_COMPILER=$(CXX) $(CMAKE) --preset default

build:
	@$(CMAKE) --build --preset default --parallel $(JOBS)

test:
	@$(CMAKE) --build --preset default --parallel $(JOBS)
	@ctest --preset default --output-on-failure

clean:
	@$(CMAKE) --build --preset default --target clean || true
	@$(CMAKE) --build --preset lint --target clean || true
	@$(CMAKE) -E rm -rf build build-lint

lint: presets
	@CC=$(CC) CXX=$(CXX) CMAKE_C_COMPILER=$(CC) CMAKE_CXX_COMPILER=$(CXX) $(CMAKE) --preset lint
	@echo "[lint] build plugin first (if available)"
	@$(CMAKE) --build --preset lint --target pycc_tidy --parallel $(JOBS) || echo "[lint] plugin not built (skipping)"
	@echo "[lint] run custom tidy target (excludes build*/ and deps)"
	@$(CMAKE) --build --preset lint --target tidy --clean-first --parallel $(JOBS)

demo: build
	@$(CMAKE) -E make_directory build/demos
	@./build/pycc -o build/demos/minimal demos/minimal.py
	@echo "[demo] Done: build/demos/minimal"
