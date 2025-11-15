#include "compiler/Compiler.h"
#include <cstring>
#include <string_view>
#include <cctype>
#include <cstdlib>

namespace pycc {

static bool equals_ci_view(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) { return false; }
  for (std::size_t i = 0; i < a.size(); ++i) {
    const unsigned char ac = static_cast<unsigned char>(a[i]);
    const unsigned char bc = static_cast<unsigned char>(b[i]);
    const char al = static_cast<char>(std::tolower(ac));
    const char bl = static_cast<char>(std::tolower(bc));
    if (al != bl) { return false; }
  }
  return true;
}

static bool is_true_value(const char* strVal) {
  if (strVal == nullptr) { return false; }
  std::string_view sv{strVal, std::strlen(strVal)};
  if (sv == "1") { return true; }
  if (equals_ci_view(sv, "true")) { return true; }
  if (equals_ci_view(sv, "yes")) { return true; }
  return false;
}

bool Compiler::use_env_color() {
  const char* env_value = std::getenv("PYCC_COLOR");
  if (env_value == nullptr) { return false; }
  return is_true_value(env_value);
}
} // namespace pycc
