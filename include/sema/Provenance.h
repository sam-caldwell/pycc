#pragma once

#include <string>

namespace pycc::sema {
    /***
     * Name: pycc::sema::Provenance
     * Purpose: Record source location where a type binding was introduced.
     */
    struct Provenance {
        std::string file;
        int line{0};
        int col{0};
    };
} // namespace pycc::sema
