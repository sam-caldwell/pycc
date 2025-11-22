/***
 * Name: pycc::opt::EffectAlias
 * Purpose: Basic effect and alias classification helpers used by other passes.
 */
#pragma once

#include "ast/Nodes.h"

namespace pycc::opt {

struct EffectAlias {
  static bool isPureExpr(const ast::Expr* e) {
    using NK = ast::NodeKind;
    if (!e) return true;
    switch (e->kind) {
      case NK::IntLiteral: case NK::FloatLiteral: case NK::BoolLiteral:
      case NK::StringLiteral: case NK::NoneLiteral:
        return true;
      case NK::Attribute: {
        // Attribute access on immutable literals is pure
        auto* a = static_cast<const ast::Attribute*>(e);
        if (!a || !a->value) return false;
        const auto k = a->value->kind;
        return (k == NK::StringLiteral || k == NK::TupleLiteral || k == NK::BytesLiteral);
      }
      case NK::Subscript: {
        // Subscript chains rooted at immutable literals with pure indices are pure.
        // Handles nested tuple indexing like (1,(2,3))[1][0].
        auto* s = static_cast<const ast::Subscript*>(e);
        if (!s) return false;
        // Verify all indices in the chain are pure
        const ast::Expr* cur = e;
        while (cur && cur->kind == NK::Subscript) {
          auto* cs = static_cast<const ast::Subscript*>(cur);
          if (!isPureExpr(cs->slice.get())) return false;
          cur = cs->value.get();
        }
        if (!cur) return false;
        const auto rootk = cur->kind;
        return (rootk == NK::StringLiteral || rootk == NK::TupleLiteral || rootk == NK::BytesLiteral);
      }
      case NK::UnaryExpr: {
        auto* u = static_cast<const ast::Unary*>(e);
        return isPureExpr(u->operand.get());
      }
      case NK::BinaryExpr: {
        auto* b = static_cast<const ast::Binary*>(e);
        return isPureExpr(b->lhs.get()) && isPureExpr(b->rhs.get());
      }
      case NK::TupleLiteral: {
        auto* t = static_cast<const ast::TupleLiteral*>(e);
        for (const auto& el : t->elements) if (!isPureExpr(el.get())) return false; return true;
      }
      case NK::ListLiteral: {
        auto* l = static_cast<const ast::ListLiteral*>(e);
        for (const auto& el : l->elements) if (!isPureExpr(el.get())) return false; return true;
      }
      default:
        return false; // calls, attributes, subscripts, comprehensions, etc. are effectful/unknown
    }
  }
  static bool isEffectfulStmt(const ast::Stmt* s) {
    using NK = ast::NodeKind;
    if (!s) return false;
    switch (s->kind) {
      case NK::ExprStmt: return !isPureExpr(static_cast<const ast::ExprStmt*>(s)->value.get());
      case NK::AssignStmt: return true; // assigns mutate program state
      case NK::ReturnStmt: return true;
      default: return true; // be conservative for control flow
    }
  }
};

} // namespace pycc::opt
