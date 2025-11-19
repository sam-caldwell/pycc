#!/usr/bin/env bash
set -uo pipefail

# Wrapper around clang-tidy that filters noisy compiler summaries
# from system headers while preserving real clang-tidy diagnostics.

# Gracefully skip if clang-tidy is not installed
if ! command -v clang-tidy >/dev/null 2>&1; then
  echo "[lint] clang-tidy not found; skipping." >&2
  exit 0
fi

clang-tidy "$@" 2>&1 | awk '
  # Drop lines like "12345 warnings generated."
  /^[0-9]+ warnings generated\.$/ { next }
  # Drop suppression summary lines from clang-tidy driver
  /^Suppressed [0-9]+ warnings/ { next }
  /^Use -header-filter=.* to display/ { next }
  { print }
'

# Propagate clang-tidy exit code
exit ${PIPESTATUS[0]}
