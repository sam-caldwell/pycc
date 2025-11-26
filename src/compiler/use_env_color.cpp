#include "compiler/Compiler.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace pycc {
    static bool equals_ci(const std::string_view lhs, const std::string_view rhs) {
        if (lhs.size() != rhs.size()) { return false; }
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            const unsigned char lhsCh = static_cast<unsigned char>(lhs[i]);
            const unsigned char rhsCh = static_cast<unsigned char>(rhs[i]);
            if (std::tolower(lhsCh) != std::tolower(rhsCh)) { return false; }
        }
        return true;
    }

    static bool is_true_value(const char *strVal) {
        if (strVal == nullptr) { return false; }
        const std::string_view valView{strVal, std::strlen(strVal)};
        if (valView == "1") { return true; }
        if (equals_ci(valView, std::string_view{"true"})) { return true; }
        if (equals_ci(valView, std::string_view{"yes"})) { return true; }
        return false;
    }

    bool Compiler::use_env_color() {
        const char *env_value = std::getenv("PYCC_COLOR");
        if (env_value == nullptr) { return false; }
        return is_true_value(env_value);
    }
} // namespace pycc
