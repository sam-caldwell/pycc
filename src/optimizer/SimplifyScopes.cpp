/***
 * Name: pycc::opt::SimplifyScopes (impl)
 */
#include "optimizer/SimplifyScopes.h"
#include "ast/IfStmt.h"
#include "ast/PassStmt.h"
#include "ast/ReturnStmt.h"
#include "ast/BoolLiteral.h"
#include "ast/IntLiteral.h"
#include "ast/FloatLiteral.h"
#include "ast/StringLiteral.h"
#include "ast/Name.h"

namespace pycc::opt {
using namespace pycc::ast;

static bool shallowEqualExpr(const Expr* a, const Expr* b) {
  if (a == nullptr || b == nullptr) return false;
  if (a->kind != b->kind) return false;
  switch (a->kind) {
    case NodeKind::IntLiteral:
      return static_cast<const IntLiteral*>(a)->value == static_cast<const IntLiteral*>(b)->value;
    case NodeKind::FloatLiteral:
      return static_cast<const FloatLiteral*>(a)->value == static_cast<const FloatLiteral*>(b)->value;
    case NodeKind::BoolLiteral:
      return static_cast<const BoolLiteral*>(a)->value == static_cast<const BoolLiteral*>(b)->value;
    case NodeKind::StringLiteral:
      return static_cast<const StringLiteral*>(a)->value == static_cast<const StringLiteral*>(b)->value;
    case NodeKind::Name:
      return static_cast<const Name*>(a)->id == static_cast<const Name*>(b)->id;
    default: break;
  }
  const auto& ca = a->canonical();
  const auto& cb = b->canonical();
  return ca && cb && *ca == *cb;
}

static std::size_t simplifyBlock(std::vector<std::unique_ptr<Stmt>>& body) {
  std::size_t changes = 0;
  std::vector<std::unique_ptr<Stmt>> out;
  out.reserve(body.size());
  for (auto& s : body) {
    if (!s) continue;
    // Drop Pass statements
    if (s->kind == NodeKind::PassStmt) { ++changes; continue; }
    if (s->kind == NodeKind::IfStmt) {
      auto* ifs = static_cast<IfStmt*>(s.get());
      changes += simplifyBlock(ifs->thenBody);
      changes += simplifyBlock(ifs->elseBody);
      // If both branches are a single return with identical expressions, replace the if with the return
      if (ifs->thenBody.size() == 1 && ifs->elseBody.size() == 1 &&
          ifs->thenBody[0] && ifs->elseBody[0] &&
          ifs->thenBody[0]->kind == NodeKind::ReturnStmt && ifs->elseBody[0]->kind == NodeKind::ReturnStmt) {
        auto* rt = static_cast<ReturnStmt*>(ifs->thenBody[0].get());
        auto* re = static_cast<ReturnStmt*>(ifs->elseBody[0].get());
        if (rt->value && re->value && shallowEqualExpr(rt->value.get(), re->value.get())) {
          out.emplace_back(std::move(ifs->thenBody[0]));
          ++changes;
          continue;
        }
      }
    }
    out.emplace_back(std::move(s));
  }
  body = std::move(out);
  return changes;
}

std::size_t SimplifyScopes::run(Module& module) {
  std::size_t changes = 0;
  for (auto& fn : module.functions) { changes += simplifyBlock(fn->body); }
  return changes;
}

} // namespace pycc::opt

