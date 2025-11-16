#pragma once

#include <string>
#include "ast/Acceptable.h"
#include "ast/Expr.h"
#include "ast/HasBody.h"
#include "ast/HasParams.h"
#include "ast/HasName.h"
#include "ast/Param.h"
#include "ast/Stmt.h"
#include "TypeKind.h"

namespace pycc::ast {
    struct FunctionDef final : Node, Acceptable<FunctionDef, NodeKind::FunctionDef>, HasBody<Stmt>, HasParams<Param>, HasName {
        TypeKind returnType{TypeKind::NoneType};
        std::vector<std::unique_ptr<Expr>> decorators; // optional decorator expressions
        FunctionDef(std::string n, const TypeKind rt)
            : Node(NodeKind::FunctionDef), HasName{std::move(n)}, returnType(rt) {}
    };

} // namespace pycc::ast
