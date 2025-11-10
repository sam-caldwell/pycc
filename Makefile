# (c) 2025 Sam Caldwell. See LICENSE.txt
# pycc — CMake/Ninja convenience wrapper

SHELL := /bin/sh

# Configurable variables (override via environment or CLI, e.g., `make BUILD_TYPE=Release build`)
BUILD_DIR ?= build
BUILD_TYPE ?= Debug
GENERATOR ?= Ninja
C_COMPILER ?= clang
CXX_COMPILER ?= clang++
CMAKE ?= cmake
TIDY ?= clang-tidy
LINT_CONFIG ?= .clang-tidy
CMAKE_FLAGS ?=
JOBS ?=

TIDY_ARGS := $(TIDY)\;--config-file=$(abspath $(LINT_CONFIG))

CONFIGURE_ARGS = -S . -B $(BUILD_DIR) -G $(GENERATOR) \
  -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
  -DCMAKE_C_COMPILER=$(C_COMPILER) \
  -DCMAKE_CXX_COMPILER=$(CXX_COMPILER) \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  $(CMAKE_FLAGS)

.PHONY: configure build clean lint demo

configure:
	@echo "[configure] $(BUILD_TYPE) -> $(BUILD_DIR) (generator=$(GENERATOR))"
	@$(CMAKE) $(CONFIGURE_ARGS)

build: configure
	@echo "[build] Building in $(BUILD_DIR)"
	@$(CMAKE) --build $(BUILD_DIR) --parallel $(JOBS)

clean:
	@echo "[clean] Removing $(BUILD_DIR)/"
	@$(CMAKE) -E remove_directory $(BUILD_DIR)

lint:
	@echo "[lint] configure (enable plugins)"
	@$(CMAKE) $(CONFIGURE_ARGS) -DPYCC_BUILD_TIDY_PLUGINS=ON -DCMAKE_CXX_CLANG_TIDY=
	@echo "[lint] build clang-tidy plugin (pycc_tidy)"
	@$(CMAKE) --build $(BUILD_DIR) --target pycc_tidy --parallel $(JOBS) || echo "[lint] warning: plugin build failed; proceeding without plugin"
	@echo "[lint] detect plugin path from CMake cache"
	@PYCC_PLUGIN_PATH=`$(CMAKE) -L -N -B $(BUILD_DIR) 2>/dev/null | sed -n 's/^PYCC_TIDY_PLUGIN_PATH:\w\+=\(.*\)/\1/p'` ; \
	 if [ -n "$$PYCC_PLUGIN_PATH" ] && [ -f "$$PYCC_PLUGIN_PATH" ]; then \
	   echo "[lint] plugin: $$PYCC_PLUGIN_PATH"; \
	   echo "[lint] reconfigure with clang-tidy + plugin + $(LINT_CONFIG)"; \
	   $(CMAKE) $(CONFIGURE_ARGS) -DCMAKE_CXX_CLANG_TIDY="$(TIDY);--load=$$PYCC_PLUGIN_PATH;--config-file=$(abspath $(LINT_CONFIG))"; \
	 else \
	   echo "[lint] plugin not available; reconfigure with built-in clang-tidy only"; \
	   $(CMAKE) $(CONFIGURE_ARGS) -DCMAKE_CXX_CLANG_TIDY=$(TIDY_ARGS); \
	 fi; \
	 echo "[lint] build (clang-tidy runs via CMake rules)"; \
	 $(CMAKE) --build $(BUILD_DIR) --parallel $(JOBS)

demo: build
	@echo "[demo] Building demos/minimal -> build/demos/minimal"
	@$(CMAKE) -E make_directory $(BUILD_DIR)/demos
	@./$(BUILD_DIR)/pycc -o $(BUILD_DIR)/demos/minimal demos/minimal.py
	@echo "[demo] Done: $(BUILD_DIR)/demos/minimal"
