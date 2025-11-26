/**
 * @file
 * @brief AST geometry visitor implementation.
 */
/***
 * Name: pycc::ast::GeometryVisitor
 * Purpose: AST visitor computing node count and max depth.
 */
#include "ast/GeometryVisitor.h"

#include "ast/AssignStmt.h"
#include "ast/Binary.h"
#include "ast/BoolLiteral.h"
#include "ast/Call.h"
#include "ast/ExprStmt.h"
#include "ast/FloatLiteral.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/IntLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/NoneLiteral.h"
#include "ast/ObjectLiteral.h"
#include "ast/ReturnStmt.h"
#include "ast/StringLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/Unary.h"

#include <algorithm>

namespace pycc::ast {
    void GeometryVisitor::bump() {
        ++nodes;
        maxDepth = std::max(maxDepth, depth);
    }

    GeometryVisitor::DepthScope::DepthScope(uint64_t &ref) : d(ref) { ++d; }
    GeometryVisitor::DepthScope::~DepthScope() { --d; }

    void GeometryVisitor::visit(const Module &module) {
        bump();
        for (const auto &func: module.functions) {
            const DepthScope scope{depth};
            func->accept(*this);
        }
    }

    void GeometryVisitor::visit(const FunctionDef &func) {
        bump();
        for (const auto &stmt: func.body) {
            const DepthScope scope{depth};
            stmt->accept(*this);
        }
    }

    void GeometryVisitor::visit(const ReturnStmt &ret) {
        bump();
        const DepthScope scope{depth};
        if (ret.value) { ret.value->accept(*this); }
    }

    void GeometryVisitor::visit(const AssignStmt &asg) {
        bump();
        const DepthScope scope{depth};
        if (asg.value) { asg.value->accept(*this); }
    }

    void GeometryVisitor::visit(const ExprStmt &expr) {
        bump();
        const DepthScope scope{depth};
        if (expr.value) { expr.value->accept(*this); }
    }

    void GeometryVisitor::visit(const IfStmt &iff) {
        bump();
        {
            const DepthScope scope{depth};
            if (iff.cond) { iff.cond->accept(*this); }
        }
        for (const auto &stmtThen: iff.thenBody) {
            const DepthScope scope{depth};
            stmtThen->accept(*this);
        }
        for (const auto &stmtElse: iff.elseBody) {
            const DepthScope scope{depth};
            stmtElse->accept(*this);
        }
    }

    void GeometryVisitor::visit(const Literal<long long, NodeKind::IntLiteral> &intLiteral) {
        (void) intLiteral;
        bump();
    }

    void GeometryVisitor::visit(const Literal<bool, NodeKind::BoolLiteral> &boolLiteral) {
        (void) boolLiteral;
        bump();
    }

    void GeometryVisitor::visit(const Literal<double, NodeKind::FloatLiteral> &floatLiteral) {
        (void) floatLiteral;
        bump();
    }

    void GeometryVisitor::visit(const Literal<std::string, NodeKind::StringLiteral> &stringLiteral) {
        (void) stringLiteral;
        bump();
    }

    void GeometryVisitor::visit(const NoneLiteral &noneLiteral) {
        (void) noneLiteral;
        bump();
    }

    void GeometryVisitor::visit(const Unary &unary) {
        bump();
        const DepthScope scope{depth};
        if (unary.operand) { unary.operand->accept(*this); }
    }

    void GeometryVisitor::visit(const TupleLiteral &tuple) {
        bump();
        for (const auto &elem: tuple.elements) {
            const DepthScope scope{depth};
            elem->accept(*this);
        }
    }

    void GeometryVisitor::visit(const ListLiteral &list) {
        bump();
        for (const auto &elem: list.elements) {
            const DepthScope scope{depth};
            elem->accept(*this);
        }
    }

    void GeometryVisitor::visit(const ObjectLiteral &obj) {
        bump();
        for (const auto &field: obj.fields) {
            const DepthScope scope{depth};
            field->accept(*this);
        }
    }

    void GeometryVisitor::visit(const Name &name) {
        (void) name;
        bump();
    }

    void GeometryVisitor::visit(const Call &call) {
        bump();
        {
            const DepthScope scope{depth};
            if (call.callee) { call.callee->accept(*this); }
        }
        for (const auto &arg: call.args) {
            const DepthScope scope{depth};
            arg->accept(*this);
        }
    }

    void GeometryVisitor::visit(const Binary &bin) {
        bump();
        {
            const DepthScope scope{depth};
            if (bin.lhs) { bin.lhs->accept(*this); }
        }
        {
            const DepthScope scope{depth};
            if (bin.rhs) { bin.rhs->accept(*this); }
        }
    }
} // namespace pycc::ast
