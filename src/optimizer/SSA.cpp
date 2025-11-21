/***
 * Name: pycc::opt::SSA (impl)
 */
#include "optimizer/SSA.h"

#include "optimizer/EffectAlias.h"
#include "ast/Module.h"
#include "ast/FunctionDef.h"
#include "ast/ExprStmt.h"
#include "ast/AssignStmt.h"
#include "ast/Binary.h"
#include "ast/Unary.h"
#include "ast/TupleLiteral.h"

namespace pycc::opt {
using namespace pycc::ast;

namespace {
static void countExpr(const Expr* e, std::size_t& vals, std::size_t& insts) {
  if (!e) return;
  switch (e->kind) {
    case NodeKind::IntLiteral:
    case NodeKind::FloatLiteral:
    case NodeKind::BoolLiteral:
    case NodeKind::StringLiteral:
    case NodeKind::Name:
      ++vals; break;
    case NodeKind::UnaryExpr:
      ++insts; ++vals; countExpr(static_cast<const Unary*>(e)->operand.get(), vals, insts); break;
    case NodeKind::BinaryExpr: {
      ++insts; ++vals; auto* b = static_cast<const Binary*>(e); countExpr(b->lhs.get(), vals, insts); countExpr(b->rhs.get(), vals, insts); break;
    }
    case NodeKind::TupleLiteral: {
      ++vals; auto* t = static_cast<const TupleLiteral*>(e); for (const auto& el : t->elements) countExpr(el.get(), vals, insts); break;
    }
    default: break;
  }
}
} // namespace

SSAStats SSA::analyze(const Module& module) {
  SSAStats s{}; s.blocks = 0;
  for (const auto& fn : module.functions) {
    ++s.blocks; // entry
    for (const auto& st : fn->body) {
      if (!st) continue;
      if (st->kind == NodeKind::ExprStmt) { const auto* es = static_cast<const ExprStmt*>(st.get()); countExpr(es->value.get(), s.values, s.instructions); }
      else if (st->kind == NodeKind::AssignStmt) { const auto* as = static_cast<const AssignStmt*>(st.get()); countExpr(as->value.get(), s.values, s.instructions); }
      else { ++s.blocks; }
    }
  }
  return s;
}

} // namespace pycc::opt
