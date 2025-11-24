/**
 * @file
 * @brief Semantic type kind enumeration used in AST annotations.
 */
#pragma once

namespace pycc::ast {
    enum class TypeKind {
        Int,
        Bool,
        Float,
        Str,
        Bytes,
        NoneType,
        Tuple,
        List,
        Dict,
        Optional,
        Union
    };

    inline const char *to_string(const TypeKind element) {
        switch (element) {
            case TypeKind::Int: return "Int";
            case TypeKind::Bool: return "Bool";
            case TypeKind::Float: return "Float";
            case TypeKind::Str: return "Str";
            case TypeKind::Bytes: return "Bytes";
            case TypeKind::NoneType: return "NoneType";
            case TypeKind::Tuple: return "Tuple";
            case TypeKind::List: return "List";
            case TypeKind::Dict: return "Dict";
            case TypeKind::Optional: return "Optional";
            case TypeKind::Union: return "Union";
            default: return "unknown";
        }
    }
} // namespace pycc::ast
