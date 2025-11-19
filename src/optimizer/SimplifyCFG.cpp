/***
 * Name: pycc::opt::SimplifyCFG (impl)
 */
#include "optimizer/SimplifyCFG.h"
#include "ast/IfStmt.h"
#include "ast/BoolLiteral.h"
#include "ast/Binary.h"
#include "ast/Unary.h"
#include "ast/BinaryOperator.h"
#include "ast/UnaryOperator.h"
#include <memory>
#include <utility>
#include <vector>

namespace pycc::opt {
using namespace pycc::ast;

// Attempt to evaluate a boolean expression tree composed of BoolLiteral,
// unary 'not', and binary 'and'/'or'. Applies short-circuit rules so that
// (True or X) -> true and (False and X) -> false without evaluating X.
static bool evalBoolExpr(const Expr* e, bool& out) {
  if (!e) return false;
  if (e->kind == NodeKind::BoolLiteral) { out = static_cast<const BoolLiteral*>(e)->value; return true; }
  if (e->kind == NodeKind::UnaryExpr) {
    const auto* u = static_cast<const Unary*>(e);
    if (u->op != UnaryOperator::Not) return false;
    bool sub{}; if (!evalBoolExpr(u->operand.get(), sub)) return false; out = !sub; return true;
  }
  if (e->kind == NodeKind::BinaryExpr) {
    const auto* b = static_cast<const Binary*>(e);
    if (b->op == BinaryOperator::And) {
      bool lv{}; if (evalBoolExpr(b->lhs.get(), lv)) {
        if (!lv) { out = false; return true; } // False and X -> False
        bool rv{}; if (evalBoolExpr(b->rhs.get(), rv)) { out = (lv && rv); return true; }
        return false; // True and unknown
      }
      bool rv{}; if (evalBoolExpr(b->rhs.get(), rv)) {
        if (!rv) { out = false; return true; } // X and False -> False
      }
      return false;
    }
    if (b->op == BinaryOperator::Or) {
      bool lv{}; if (evalBoolExpr(b->lhs.get(), lv)) {
        if (lv) { out = true; return true; } // True or X -> True
        bool rv{}; if (evalBoolExpr(b->rhs.get(), rv)) { out = (lv || rv); return true; }
        return false; // False or unknown
      }
      bool rv{}; if (evalBoolExpr(b->rhs.get(), rv)) {
        if (rv) { out = true; return true; } // unknown or True -> True
      }
      return false;
    }
  }
  return false;
}
static void simplifyBlock(std::vector<std::unique_ptr<Stmt>>& body, size_t& pruned) {
  std::vector<std::unique_ptr<Stmt>> out;
  out.reserve(body.size());
  for (auto& s : body) {
    if (s->kind == NodeKind::IfStmt) {
      auto* ifs = static_cast<IfStmt*>(s.get());
      simplifyBlock(ifs->thenBody, pruned);
      simplifyBlock(ifs->elseBody, pruned);
      bool v = false;
      if (ifs->cond && evalBoolExpr(ifs->cond.get(), v)) {
        if (v) { for (auto& t : ifs->thenBody) out.emplace_back(std::move(t)); }
        else { for (auto& e : ifs->elseBody) out.emplace_back(std::move(e)); }
        ++pruned;
        continue;
      }
      // If both branches are empty, drop the if entirely
      if (ifs->thenBody.empty() && ifs->elseBody.empty()) {
        ++pruned; // removed empty if
        continue;
      }
      // If then is empty but else is not, invert condition and swap bodies to canonicalize
      if (ifs->thenBody.empty() && !ifs->elseBody.empty()) {
        if (ifs->cond) {
          std::unique_ptr<Expr> old = std::move(ifs->cond);
          auto inv = std::make_unique<Unary>(UnaryOperator::Not, std::move(old));
          ifs->cond = std::move(inv);
        }
        std::swap(ifs->thenBody, ifs->elseBody);
        ++pruned; // count as a simplification
      }
      out.emplace_back(std::move(s));
    } else {
      out.emplace_back(std::move(s));
    }
  }
  body = std::move(out);
}

size_t SimplifyCFG::run(Module& module) {
  stats_.clear();
  size_t pruned = 0;
  for (auto& f : module.functions) {
    simplifyBlock(f->body, pruned);
  }
  stats_["pruned_if"] = pruned;
  return pruned;
}

} // namespace pycc::opt
