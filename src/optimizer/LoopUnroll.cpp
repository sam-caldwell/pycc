/***
 * Name: pycc::opt::LoopUnroll (impl)
 */
#include "optimizer/LoopUnroll.h"

#include "ast/Module.h"
#include "ast/FunctionDef.h"
#include "ast/ForStmt.h"
#include "ast/AssignStmt.h"
#include "ast/ExprStmt.h"
#include "ast/Name.h"
#include "ast/Call.h"
#include "ast/IntLiteral.h"
#include "ast/BoolLiteral.h"
#include "ast/FloatLiteral.h"
#include "ast/StringLiteral.h"
#include "ast/Binary.h"
#include "ast/Unary.h"
#include "ast/TupleLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/BreakStmt.h"
#include "ast/ContinueStmt.h"
#include "ast/WhileStmt.h"
#include "ast/TryStmt.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>
#include <iterator>

namespace pycc::opt {
using namespace pycc::ast;

namespace {

// Minimal expression cloner sufficient for simple unrolling scenarios
static std::unique_ptr<Expr> cloneExpr(const Expr* e) {
  if (!e) return nullptr;
  switch (e->kind) {
    case NodeKind::IntLiteral: return std::make_unique<IntLiteral>(static_cast<const IntLiteral*>(e)->value);
    case NodeKind::FloatLiteral: return std::make_unique<FloatLiteral>(static_cast<const FloatLiteral*>(e)->value);
    case NodeKind::BoolLiteral: return std::make_unique<BoolLiteral>(static_cast<const BoolLiteral*>(e)->value);
    case NodeKind::StringLiteral: return std::make_unique<StringLiteral>(static_cast<const StringLiteral*>(e)->value);
    case NodeKind::Name: return std::make_unique<Name>(static_cast<const Name*>(e)->id);
    case NodeKind::UnaryExpr: {
      auto* u = static_cast<const Unary*>(e);
      return std::make_unique<Unary>(u->op, cloneExpr(u->operand.get()));
    }
    case NodeKind::BinaryExpr: {
      auto* b = static_cast<const Binary*>(e);
      return std::make_unique<Binary>(b->op, cloneExpr(b->lhs.get()), cloneExpr(b->rhs.get()));
    }
    case NodeKind::TupleLiteral: {
      auto* t = static_cast<const TupleLiteral*>(e);
      auto c = std::make_unique<TupleLiteral>();
      for (const auto& el : t->elements) c->elements.emplace_back(cloneExpr(el.get()));
      return c;
    }
    case NodeKind::ListLiteral: {
      auto* l = static_cast<const ListLiteral*>(e);
      auto c = std::make_unique<ListLiteral>();
      for (const auto& el : l->elements) c->elements.emplace_back(cloneExpr(el.get()));
      return c;
    }
    default: return nullptr;
  }
}

static std::unique_ptr<Stmt> cloneStmt(const Stmt* s) {
  if (!s) return nullptr;
  switch (s->kind) {
    case NodeKind::ExprStmt: {
      const auto* es = static_cast<const ExprStmt*>(s);
      auto c = std::make_unique<ExprStmt>(cloneExpr(es->value.get()));
      c->line = s->line; c->col = s->col; c->file = s->file; return c;
    }
    case NodeKind::AssignStmt: {
      const auto* as = static_cast<const AssignStmt*>(s);
      if (!as->targets.empty()) {
        // Only support simple single-name target in the new targets[] form
        if (as->targets.size() == 1 && as->targets[0] && as->targets[0]->kind == NodeKind::Name) {
          const auto* nm = static_cast<const Name*>(as->targets[0].get());
          auto c = std::make_unique<AssignStmt>(nm->id, cloneExpr(as->value.get()));
          c->line = s->line; c->col = s->col; c->file = s->file; return c;
        }
        return nullptr;
      }
      auto c = std::make_unique<AssignStmt>(as->target, cloneExpr(as->value.get()));
      c->line = s->line; c->col = s->col; c->file = s->file; return c;
    }
    default: return nullptr;
  }
}

static bool parseRange(const Expr* iter, int64_t& start, int64_t& stop, int64_t& step) {
  if (!iter || iter->kind != NodeKind::Call) return false;
  const auto* call = static_cast<const Call*>(iter);
  if (!call->callee || call->callee->kind != NodeKind::Name) return false;
  const auto* nm = static_cast<const Name*>(call->callee.get());
  if (nm->id != "range") return false;
  const auto argInt = [](const std::unique_ptr<Expr>& e, int64_t& out) -> bool {
    if (!e) return false;
    if (e->kind == NodeKind::IntLiteral) { out = static_cast<const IntLiteral*>(e.get())->value; return true; }
    return false;
  };
  step = 1; start = 0; stop = 0;
  if (call->args.size() == 1) { return argInt(call->args[0], stop); }
  if (call->args.size() == 2) { return argInt(call->args[0], start) && argInt(call->args[1], stop); }
  if (call->args.size() == 3) { return argInt(call->args[0], start) && argInt(call->args[1], stop) && argInt(call->args[2], step); }
  return false;
}

static bool bodySafeAndClonable(const std::vector<std::unique_ptr<Stmt>>& body) {
  for (const auto& s : body) {
    if (!s) continue;
    switch (s->kind) {
      case NodeKind::ExprStmt:
      case NodeKind::AssignStmt:
        // Ensure we can clone expressions/assignments
        if (!cloneStmt(s.get())) return false;
        break;
      default:
        return false; // conservatively reject complex control flow (break/continue/loops/try/etc.)
    }
  }
  return true;
}

static std::size_t runOnBlock(std::vector<std::unique_ptr<Stmt>>& body) {
  const int kMaxUnroll = 8;
  std::size_t changes = 0;
  for (std::size_t i = 0; i < body.size(); ++i) {
    auto* st = body[i].get(); if (!st || st->kind != NodeKind::ForStmt) continue;
    auto* fs = static_cast<ForStmt*>(st);
    // Only simple name targets supported
    if (!fs->target || fs->target->kind != NodeKind::Name) continue;
    const auto* tgt = static_cast<const Name*>(fs->target.get());
    int64_t start = 0, stop = 0, step = 1;
    if (!parseRange(fs->iterable.get(), start, stop, step)) continue;
    if (step == 0) continue;
    if (step < 0) continue; // only handle positive step for now
    if (stop <= start) { // zero or negative iterations: remove loop and just emit else
      // Replace loop with else-body (cloned)
      std::vector<std::unique_ptr<Stmt>> repl;
      for (const auto& s2 : fs->elseBody) { auto c = cloneStmt(s2.get()); if (c) repl.emplace_back(std::move(c)); else { repl.clear(); break; } }
      if (!repl.empty()) {
        body.erase(body.begin() + static_cast<long>(i));
        body.insert(body.begin() + static_cast<long>(i), std::make_move_iterator(repl.begin()), std::make_move_iterator(repl.end()));
        i += repl.size() ? (repl.size() - 1) : 0; ++changes;
      }
      continue;
    }
    const int64_t nIters = (stop - start + step - 1) / step;
    if (nIters <= 0 || nIters > kMaxUnroll) continue;
    if (!bodySafeAndClonable(fs->thenBody)) continue;
    // Crude cost model: unroll only for small bodies
    const std::size_t stmtCount = fs->thenBody.size();
    bool okCost = (stmtCount <= 2 && nIters <= 8) || (stmtCount <= 4 && nIters <= 4) || (stmtCount <= 1 && nIters <= 16);
    if (!okCost) continue;

    // Build replacement sequence
    std::vector<std::unique_ptr<Stmt>> repl;
    repl.reserve(static_cast<std::size_t>(nIters) * (stmtCount + 1) + fs->elseBody.size());
    for (int64_t k = 0, v = start; k < nIters; ++k, v += step) {
      auto assign = std::make_unique<AssignStmt>(tgt->id, std::make_unique<IntLiteral>(v));
      assign->line = fs->line; assign->col = fs->col; assign->file = fs->file;
      repl.emplace_back(std::move(assign));
      for (const auto& s2 : fs->thenBody) {
        auto c = cloneStmt(s2.get()); if (!c) { repl.clear(); break; }
        repl.emplace_back(std::move(c));
      }
      if (repl.empty()) break;
    }
    if (repl.empty()) continue; // failed to clone
    for (const auto& s2 : fs->elseBody) {
      auto c = cloneStmt(s2.get()); if (!c) { repl.clear(); break; }
      repl.emplace_back(std::move(c));
    }
    if (repl.empty()) continue;

    // Replace the loop with the unrolled sequence
    body.erase(body.begin() + static_cast<long>(i));
    body.insert(body.begin() + static_cast<long>(i), std::make_move_iterator(repl.begin()), std::make_move_iterator(repl.end()));
    i += repl.size() ? (repl.size() - 1) : 0;
    ++changes;
  }
  return changes;
}

} // namespace

std::size_t LoopUnroll::run(Module& module) {
  std::size_t total = 0;
  for (auto& fn : module.functions) { total += runOnBlock(fn->body); }
  return total;
}

} // namespace pycc::opt
