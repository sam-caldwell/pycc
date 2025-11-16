#pragma once

#include <memory>
#include <string>
#include <vector>
#include "ast/Node.h"
#include "ast/HasBody.h"
#include "ast/HasName.h"
#include "ast/Acceptable.h"
#include "ast/Expr.h"
#include "ast/Stmt.h"

namespace pycc::ast {
    struct ClassDef final : Stmt, Acceptable<ClassDef, NodeKind::ClassDef>, HasBody<Stmt>, HasName {
        std::vector<std::unique_ptr<Expr>> bases;       // positional bases only (shape-only)
        std::vector<std::unique_ptr<Expr>> decorators;  // decorator expressions
        explicit ClassDef(std::string n) : Stmt(NodeKind::ClassDef), HasName{std::move(n)} {}
    };
}
