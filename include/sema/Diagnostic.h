/***
 * Name: pycc::sema::Diagnostic
 * Purpose: Carry a diagnostic message with optional source location.
 */
#pragma once

#include <string>

namespace pycc::sema {
    struct Diagnostic {
        std::string message;
        std::string file;
        int line{0};
        int col{0};
    };
} // namespace pycc::sema
