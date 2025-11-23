/***
 * Name: pycc::rt::TypeTag
 * Purpose: Tags used by the runtime to identify heap object kinds.
 */
#pragma once

#include <cstdint>

namespace pycc::rt {
    enum class TypeTag : uint32_t {
        String = 1,
        Int = 2,
        Float = 3,
        Bool = 4,
        List = 5,
        Object = 6,
        Dict = 7,
        Bytes = 8,
        ByteArray = 9
    };
} // namespace pycc::rt
