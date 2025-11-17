#pragma once

#include <string>
#include <memory>
#include "TypeKind.h"


namespace pycc::ast {
    struct Expr; // fwd
    struct Param {
        std::string name;
        TypeKind type{TypeKind::NoneType};
        std::unique_ptr<Expr> defaultValue{}; // optional
        bool isVarArg{false};   // *args
        bool isKwVarArg{false}; // **kwargs
        bool isKwOnly{false};   // kw-only param (after bare *)
    };
} // namespace pycc::ast
