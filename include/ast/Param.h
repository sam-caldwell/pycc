#pragma once

#include <string>
#include "TypeKind.h"


namespace pycc::ast {
    struct Param {
        std::string name;
        TypeKind type{TypeKind::NoneType};
    };
} // namespace pycc::ast
