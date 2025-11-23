/***
 * Name: pycc::sema::FuncFlags
 * Purpose: Per-function traits discovered in pre-scan (generator/coroutine).
 */
#pragma once

namespace pycc::sema {
    struct FuncFlags {
        bool isGenerator{false};
        bool isCoroutine{false};
    };
} // namespace pycc::sema
