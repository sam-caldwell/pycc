/***
 * Name: pycc::opt::ConstantFold (impl)
 * Purpose: Fold constant expressions via AST mutation using visitors.
 */
#include "optimizer/ConstantFold.h"
#include <memory>

namespace pycc::opt {

using namespace pycc::ast;

namespace {

template <typename T>
static bool isLit(const Expr* /*e*/) { return false; }

template <>
inline bool isLit<IntLiteral>(const Expr* e) { return e && e->kind == NodeKind::IntLiteral; }
// Silence unused function warning for BoolLiteral specialization by using it
template <>
inline bool isLit<FloatLiteral>(const Expr* e) { return e && e->kind == NodeKind::FloatLiteral; }

static bool foldExpr(std::unique_ptr<Expr>& e, size_t& changes, std::unordered_map<std::string,uint64_t>& stats);

static bool foldUnary(Unary& u, std::unique_ptr<Expr>& holder, size_t& changes, std::unordered_map<std::string,uint64_t>& stats) {
  if (u.operand) foldExpr(u.operand, changes, stats);
  if (u.op == UnaryOperator::Neg) {
    if (isLit<IntLiteral>(u.operand.get())) {
      auto* lit = static_cast<IntLiteral*>(u.operand.get());
      holder = std::make_unique<IntLiteral>(-lit->value);
      ++changes; stats["unary"]++; return true;
    }
    if (isLit<FloatLiteral>(u.operand.get())) {
      auto* lit = static_cast<FloatLiteral*>(u.operand.get());
      holder = std::make_unique<FloatLiteral>(-lit->value);
      ++changes; stats["unary"]++; return true;
    }
  }
  return false;
}

static bool foldBinary(Binary& b, std::unique_ptr<Expr>& holder, size_t& changes, std::unordered_map<std::string,uint64_t>& stats) {
  if (b.lhs) foldExpr(b.lhs, changes, stats);
  if (b.rhs) foldExpr(b.rhs, changes, stats);
  auto* L = b.lhs.get();
  auto* R = b.rhs.get();
  // int ops
  if (isLit<IntLiteral>(L) && isLit<IntLiteral>(R)) {
    auto* li = static_cast<IntLiteral*>(L);
    auto* ri = static_cast<IntLiteral*>(R);
    long long v = 0;
    switch (b.op) {
      case BinaryOperator::Add: v = li->value + ri->value; break;
      case BinaryOperator::Sub: v = li->value - ri->value; break;
      case BinaryOperator::Mul: v = li->value * ri->value; break;
      case BinaryOperator::Div: if (ri->value == 0) return false; v = li->value / ri->value; break;
      case BinaryOperator::Mod: if (ri->value == 0) return false; v = li->value % ri->value; break;
      case BinaryOperator::Eq: holder = std::make_unique<BoolLiteral>(li->value == ri->value); ++changes; stats["compare_int"]++; return true;
      case BinaryOperator::Ne: holder = std::make_unique<BoolLiteral>(li->value != ri->value); ++changes; stats["compare_int"]++; return true;
      case BinaryOperator::Lt: holder = std::make_unique<BoolLiteral>(li->value < ri->value); ++changes; stats["compare_int"]++; return true;
      case BinaryOperator::Le: holder = std::make_unique<BoolLiteral>(li->value <= ri->value); ++changes; stats["compare_int"]++; return true;
      case BinaryOperator::Gt: holder = std::make_unique<BoolLiteral>(li->value > ri->value); ++changes; stats["compare_int"]++; return true;
      case BinaryOperator::Ge: holder = std::make_unique<BoolLiteral>(li->value >= ri->value); ++changes; stats["compare_int"]++; return true;
      default: return false;
    }
    holder = std::make_unique<IntLiteral>(v);
    ++changes; stats["binary_int"]++; return true;
  }
  // float ops
  if (isLit<FloatLiteral>(L) && isLit<FloatLiteral>(R)) {
    auto* lf = static_cast<FloatLiteral*>(L);
    auto* rf = static_cast<FloatLiteral*>(R);
    double v = 0.0;
    switch (b.op) {
      case BinaryOperator::Add: v = lf->value + rf->value; break;
      case BinaryOperator::Sub: v = lf->value - rf->value; break;
      case BinaryOperator::Mul: v = lf->value * rf->value; break;
      case BinaryOperator::Div: if (rf->value == 0.0) return false; v = lf->value / rf->value; break;
      case BinaryOperator::Eq: holder = std::make_unique<BoolLiteral>(lf->value == rf->value); ++changes; stats["compare_float"]++; return true;
      case BinaryOperator::Ne: holder = std::make_unique<BoolLiteral>(lf->value != rf->value); ++changes; stats["compare_float"]++; return true;
      case BinaryOperator::Lt: holder = std::make_unique<BoolLiteral>(lf->value < rf->value); ++changes; stats["compare_float"]++; return true;
      case BinaryOperator::Le: holder = std::make_unique<BoolLiteral>(lf->value <= rf->value); ++changes; stats["compare_float"]++; return true;
      case BinaryOperator::Gt: holder = std::make_unique<BoolLiteral>(lf->value > rf->value); ++changes; stats["compare_float"]++; return true;
      case BinaryOperator::Ge: holder = std::make_unique<BoolLiteral>(lf->value >= rf->value); ++changes; stats["compare_float"]++; return true;
      default: return false;
    }
    holder = std::make_unique<FloatLiteral>(v);
    ++changes; stats["binary_float"]++; return true;
  }
  return false;
}

static bool foldExpr(std::unique_ptr<Expr>& e, size_t& changes, std::unordered_map<std::string,uint64_t>& stats) {
  if (!e) return false;
  switch (e->kind) {
    case NodeKind::UnaryExpr: {
      auto* u = static_cast<Unary*>(e.get());
      std::unique_ptr<Expr> rep;
      if (foldUnary(*u, rep, changes, stats)) { rep->file = e->file; rep->line = e->line; rep->col = e->col; e = std::move(rep); return true; }
      return false;
    }
    case NodeKind::BinaryExpr: {
      auto* b = static_cast<Binary*>(e.get());
      std::unique_ptr<Expr> rep;
      if (foldBinary(*b, rep, changes, stats)) { rep->file = e->file; rep->line = e->line; rep->col = e->col; e = std::move(rep); return true; }
      return false;
    }
    case NodeKind::Call: {
      auto* c = static_cast<Call*>(e.get());
      if (c->callee) foldExpr(c->callee, changes, stats);
      for (auto& a : c->args) foldExpr(a, changes, stats);
      return false;
    }
    default:
      return false;
  }
}

} // namespace

size_t ConstantFold::run(Module& m) {
  size_t changes = 0;
  stats_.clear();
  for (auto& fn : m.functions) {
    for (auto& st : fn->body) {
      switch (st->kind) {
        case NodeKind::AssignStmt: {
          auto* a = static_cast<AssignStmt*>(st.get());
          foldExpr(a->value, changes, stats_);
          break;
        }
        case NodeKind::ReturnStmt: {
          auto* r = static_cast<ReturnStmt*>(st.get());
          foldExpr(r->value, changes, stats_);
          break;
        }
        case NodeKind::IfStmt: {
          auto* i = static_cast<IfStmt*>(st.get());
          foldExpr(i->cond, changes, stats_);
          for (auto& s2 : i->thenBody) {
            if (s2->kind == NodeKind::AssignStmt) {
              auto* a2 = static_cast<AssignStmt*>(s2.get()); foldExpr(a2->value, changes, stats_);
            } else if (s2->kind == NodeKind::ReturnStmt) {
              auto* r2 = static_cast<ReturnStmt*>(s2.get()); foldExpr(r2->value, changes, stats_);
            }
          }
          for (auto& s3 : i->elseBody) {
            if (s3->kind == NodeKind::AssignStmt) {
              auto* a3 = static_cast<AssignStmt*>(s3.get()); foldExpr(a3->value, changes, stats_);
            } else if (s3->kind == NodeKind::ReturnStmt) {
              auto* r3 = static_cast<ReturnStmt*>(s3.get()); foldExpr(r3->value, changes, stats_);
            }
          }
          break;
        }
        default: break;
      }
    }
  }
  return changes;
}

} // namespace pycc::opt
