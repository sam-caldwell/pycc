#include "compiler/Compiler.h"
extern "C" char* getenv(const char*);

namespace pycc {

static bool equals_ci(const char* lhs, const char* rhs) { // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  int index = 0;
  for (;;) {
    char leftChar = lhs[index];   // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    char rightChar = rhs[index];  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    if (leftChar >= 'A' && leftChar <= 'Z') { leftChar = static_cast<char>(leftChar - 'A' + 'a'); }
    if (rightChar >= 'A' && rightChar <= 'Z') { rightChar = static_cast<char>(rightChar - 'A' + 'a'); }
    if (leftChar != rightChar) { return false; }
    if (leftChar == '\0') { return true; }
    ++index;
  }
}

static bool is_true_value(const char* strVal) {
  if (strVal == nullptr) { return false; }
  if (strVal[0] == '1' && strVal[1] == '\0') { return true; } // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  if (equals_ci(strVal, "true")) { return true; }
  if (equals_ci(strVal, "yes")) { return true; }
  return false;
}

bool Compiler::use_env_color() {
  const char* env_value = nullptr;
  env_value = getenv("PYCC_COLOR"); // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
  if (env_value == nullptr) {
    return false;
  }
  return is_true_value(env_value);
}
} // namespace pycc
