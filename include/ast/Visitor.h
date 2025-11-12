#pragma once

#include "ast/Nodes.h"

namespace pycc::ast {

template <typename V>
void dispatch(Node& n, V& v) {
    switch (n.kind) {
        case NodeKind::Module: v.visit(static_cast<Module&>(n)); break;
        case NodeKind::FunctionDef: v.visit(static_cast<FunctionDef&>(n)); break;
        case NodeKind::ReturnStmt: v.visit(static_cast<ReturnStmt&>(n)); break;
        case NodeKind::AssignStmt: v.visit(static_cast<AssignStmt&>(n)); break;
        case NodeKind::ExprStmt: v.visit(static_cast<ExprStmt&>(n)); break;
        case NodeKind::IfStmt: v.visit(static_cast<IfStmt&>(n)); break;
        case NodeKind::IntLiteral: v.visit(static_cast<IntLiteral&>(n)); break;
        case NodeKind::BoolLiteral: v.visit(static_cast<BoolLiteral&>(n)); break;
        case NodeKind::FloatLiteral: v.visit(static_cast<FloatLiteral&>(n)); break;
        case NodeKind::StringLiteral: v.visit(static_cast<StringLiteral&>(n)); break;
        case NodeKind::Name: v.visit(static_cast<Name&>(n)); break;
        case NodeKind::Call: v.visit(static_cast<Call&>(n)); break;
        case NodeKind::BinaryExpr: v.visit(static_cast<Binary&>(n)); break;
        case NodeKind::UnaryExpr: v.visit(static_cast<Unary&>(n)); break;
        case NodeKind::TupleLiteral: v.visit(static_cast<TupleLiteral&>(n)); break;
        case NodeKind::ListLiteral: v.visit(static_cast<ListLiteral&>(n)); break;
        case NodeKind::NoneLiteral: v.visit(static_cast<NoneLiteral&>(n)); break;
    }
}

template <typename V>
void dispatch(const Node& n, V& v) {
    dispatch(const_cast<Node&>(n), v);
}

} // namespace pycc::ast
