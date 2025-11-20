#pragma once

#include <string>
#include <memory>
#include <vector>
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
        bool isPosOnly{false};  // positional-only (before '/')
        // Rich typing metadata (shape-only parsing):
        // - unionTypes: if specified, includes all alternatives for this param (first entry is base type)
        // - listElemType: if type is List and annotation was list[T], record T here
        std::vector<TypeKind> unionTypes{};
        TypeKind listElemType{TypeKind::NoneType};
    };
} // namespace pycc::ast
