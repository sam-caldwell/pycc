/***
 * Name: pycc::sema::Provenance
 * Purpose: Record source location where a type binding was introduced.
 */
#pragma once

#include <string>

namespace pycc::sema {
    struct Provenance {
        std::string file;
        int line{0};
        int col{0};
    };
} // namespace pycc::sema
