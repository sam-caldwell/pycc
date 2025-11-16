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
        case NodeKind::WhileStmt: v.visit(static_cast<WhileStmt&>(n)); break;
        case NodeKind::ForStmt: v.visit(static_cast<ForStmt&>(n)); break;
        case NodeKind::BreakStmt: v.visit(static_cast<BreakStmt&>(n)); break;
        case NodeKind::ContinueStmt: v.visit(static_cast<ContinueStmt&>(n)); break;
        case NodeKind::PassStmt: v.visit(static_cast<PassStmt&>(n)); break;
        case NodeKind::TryStmt: v.visit(static_cast<TryStmt&>(n)); break;
        case NodeKind::ExceptHandler: v.visit(static_cast<ExceptHandler&>(n)); break;
        case NodeKind::WithItem: v.visit(static_cast<WithItem&>(n)); break;
        case NodeKind::WithStmt: v.visit(static_cast<WithStmt&>(n)); break;
        case NodeKind::Import: v.visit(static_cast<Import&>(n)); break;
        case NodeKind::ImportFrom: v.visit(static_cast<ImportFrom&>(n)); break;
        case NodeKind::Alias: v.visit(static_cast<Alias&>(n)); break;
        case NodeKind::ClassDef: v.visit(static_cast<ClassDef&>(n)); break;
        case NodeKind::DelStmt: v.visit(static_cast<DelStmt&>(n)); break;
        case NodeKind::DefStmt: v.visit(static_cast<DefStmt&>(n)); break;
        case NodeKind::IntLiteral: v.visit(static_cast<IntLiteral&>(n)); break;
        case NodeKind::BoolLiteral: v.visit(static_cast<BoolLiteral&>(n)); break;
        case NodeKind::FloatLiteral: v.visit(static_cast<FloatLiteral&>(n)); break;
        case NodeKind::StringLiteral: v.visit(static_cast<StringLiteral&>(n)); break;
        case NodeKind::Name: v.visit(static_cast<Name&>(n)); break;
        case NodeKind::Attribute: v.visit(static_cast<Attribute&>(n)); break;
        case NodeKind::Subscript: v.visit(static_cast<Subscript&>(n)); break;
        case NodeKind::Call: v.visit(static_cast<Call&>(n)); break;
        case NodeKind::BinaryExpr: v.visit(static_cast<Binary&>(n)); break;
        case NodeKind::UnaryExpr: v.visit(static_cast<Unary&>(n)); break;
        case NodeKind::TupleLiteral: v.visit(static_cast<TupleLiteral&>(n)); break;
        case NodeKind::ListLiteral: v.visit(static_cast<ListLiteral&>(n)); break;
        case NodeKind::ObjectLiteral: v.visit(static_cast<ObjectLiteral&>(n)); break;
        case NodeKind::NoneLiteral: v.visit(static_cast<NoneLiteral&>(n)); break;
        case NodeKind::BytesLiteral: v.visit(static_cast<BytesLiteral&>(n)); break;
        case NodeKind::EllipsisLiteral: v.visit(static_cast<EllipsisLiteral&>(n)); break;
        case NodeKind::DictLiteral: v.visit(static_cast<DictLiteral&>(n)); break;
        case NodeKind::SetLiteral: v.visit(static_cast<SetLiteral&>(n)); break;
        case NodeKind::NamedExpr: v.visit(static_cast<NamedExpr&>(n)); break;
        case NodeKind::MatchStmt: v.visit(static_cast<MatchStmt&>(n)); break;
        case NodeKind::MatchCase: v.visit(static_cast<MatchCase&>(n)); break;
        case NodeKind::PatternWildcard: v.visit(static_cast<PatternWildcard&>(n)); break;
        case NodeKind::PatternName: v.visit(static_cast<PatternName&>(n)); break;
        case NodeKind::PatternLiteral: v.visit(static_cast<PatternLiteral&>(n)); break;
        case NodeKind::PatternOr: v.visit(static_cast<PatternOr&>(n)); break;
        case NodeKind::PatternAs: v.visit(static_cast<PatternAs&>(n)); break;
        case NodeKind::PatternClass: v.visit(static_cast<PatternClass&>(n)); break;
        case NodeKind::IfExpr: v.visit(static_cast<IfExpr&>(n)); break;
        case NodeKind::LambdaExpr: v.visit(static_cast<LambdaExpr&>(n)); break;
        case NodeKind::AugAssignStmt: v.visit(static_cast<AugAssignStmt&>(n)); break;
        case NodeKind::RaiseStmt: v.visit(static_cast<RaiseStmt&>(n)); break;
        case NodeKind::GlobalStmt: v.visit(static_cast<GlobalStmt&>(n)); break;
        case NodeKind::NonlocalStmt: v.visit(static_cast<NonlocalStmt&>(n)); break;
        case NodeKind::AssertStmt: v.visit(static_cast<AssertStmt&>(n)); break;
        case NodeKind::YieldExpr: v.visit(static_cast<YieldExpr&>(n)); break;
        case NodeKind::AwaitExpr: v.visit(static_cast<AwaitExpr&>(n)); break;
        case NodeKind::ListComp: v.visit(static_cast<ListComp&>(n)); break;
        case NodeKind::SetComp: v.visit(static_cast<SetComp&>(n)); break;
        case NodeKind::DictComp: v.visit(static_cast<DictComp&>(n)); break;
        case NodeKind::GeneratorExpr: v.visit(static_cast<GeneratorExpr&>(n)); break;
        default: break;
    }
}

template <typename V>
void dispatch(const Node& n, V& v) {
    dispatch(const_cast<Node&>(n), v);
}

} // namespace pycc::ast
