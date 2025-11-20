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
        // Subscript on immutable literals with pure index is pure (e.g., "abc"[0], (1,2)[1])
        auto* s = static_cast<const ast::Subscript*>(e);
        if (!s || !s->value) return false;
        const auto vk = s->value->kind;
        if (!(vk == NK::StringLiteral || vk == NK::TupleLiteral || vk == NK::BytesLiteral)) return false;
        return isPureExpr(s->slice.get());
      }
      case NK::Unary: {
        auto* u = static_cast<const ast::Unary*>(e);
        return isPureExpr(u->operand.get());
      }
      case NK::Binary: {
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
