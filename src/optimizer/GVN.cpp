/***
 * Name: pycc::opt::GVN (impl)
 */
#include "optimizer/GVN.h"

#include "optimizer/EffectAlias.h"
#include "ast/Module.h"
#include "ast/FunctionDef.h"
#include "ast/ExprStmt.h"
#include "ast/Binary.h"
#include "ast/Unary.h"
#include "ast/Attribute.h"
#include "ast/Subscript.h"
#include <unordered_map>

namespace pycc::opt {
using namespace pycc::ast;

namespace {
static void hashExpr(const Expr* e, std::string& out);
static void hashExpr(const Expr* e, std::string& out) {
  if (!e) { out += "<null>"; return; }
  out.push_back('#'); out += std::to_string(static_cast<int>(e->kind)); out.push_back(':');
  switch (e->kind) {
    case NodeKind::IntLiteral: out += std::to_string(static_cast<const IntLiteral*>(e)->value); break;
    case NodeKind::FloatLiteral: out += std::to_string(static_cast<const FloatLiteral*>(e)->value); break;
    case NodeKind::BoolLiteral: out += (static_cast<const BoolLiteral*>(e)->value ? "1" : "0"); break;
    case NodeKind::StringLiteral: out += static_cast<const StringLiteral*>(e)->value; break;
    case NodeKind::UnaryExpr: hashExpr(static_cast<const Unary*>(e)->operand.get(), out); break;
    case NodeKind::BinaryExpr: {
      const auto* b = static_cast<const Binary*>(e);
      out += std::to_string(static_cast<int>(b->op));
      hashExpr(b->lhs.get(), out); hashExpr(b->rhs.get(), out); break;
    }
    case NodeKind::TupleLiteral: {
      const auto* t = static_cast<const TupleLiteral*>(e);
      for (const auto& el : t->elements) hashExpr(el.get(), out); break;
    }
    case NodeKind::Attribute: {
      const auto* a = static_cast<const Attribute*>(e);
      hashExpr(a->value.get(), out); out += '.'; out += a->attr; break;
    }
    case NodeKind::Subscript: {
      const auto* s = static_cast<const Subscript*>(e);
      hashExpr(s->value.get(), out); out.push_back('['); hashExpr(s->slice.get(), out); out.push_back(']'); break;
    }
    default: out += "?"; break;
  }
}
} // namespace

GVN::Result GVN::analyze(const Module& module) {
  std::unordered_map<std::string, int> classes;
  std::size_t exprs = 0;
  for (const auto& fn : module.functions) {
    for (const auto& st : fn->body) {
      if (!st || st->kind != NodeKind::ExprStmt) continue;
      const auto* es = static_cast<const ExprStmt*>(st.get());
      if (!EffectAlias::isPureExpr(es->value.get())) continue;
      std::string key; hashExpr(es->value.get(), key);
      if (!key.empty()) { classes[key]++; exprs++; }
    }
  }
  Result r; r.expressions = exprs; r.classes = classes.size();
  return r;
}

} // namespace pycc::opt
