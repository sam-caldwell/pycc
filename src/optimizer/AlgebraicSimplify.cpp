/***
 * Name: pycc::opt::AlgebraicSimplify (impl)
 */
#include "optimizer/AlgebraicSimplify.h"
#include <memory>

namespace pycc::opt {
using namespace pycc::ast;

static bool isZero(const Expr* e) {
  if (!e) return false;
  if (e->kind == NodeKind::IntLiteral) return static_cast<const IntLiteral*>(e)->value == 0;
  if (e->kind == NodeKind::FloatLiteral) return static_cast<const FloatLiteral*>(e)->value == 0.0;
  return false;
}
static bool isOne(const Expr* e) {
  if (!e) return false;
  if (e->kind == NodeKind::IntLiteral) return static_cast<const IntLiteral*>(e)->value == 1;
  if (e->kind == NodeKind::FloatLiteral) return static_cast<const FloatLiteral*>(e)->value == 1.0;
  return false;
}

static bool simplifyExpr(std::unique_ptr<Expr>& e, size_t& changes, std::unordered_map<std::string,uint64_t>& stats) {
  if (!e) return false;
  switch (e->kind) {
    case NodeKind::BinaryExpr: {
      auto* b = static_cast<Binary*>(e.get());
      simplifyExpr(b->lhs, changes, stats);
      simplifyExpr(b->rhs, changes, stats);
      const Expr* L = b->lhs.get(); const Expr* R = b->rhs.get();
      if (L && R) {
        // Integers
        if (L->kind == NodeKind::IntLiteral || R->kind == NodeKind::IntLiteral) {
          switch (b->op) {
            case BinaryOperator::Add:
              if (isZero(L)) { e = std::move(b->rhs); ++changes; stats["algebraic_int"]++; return true; }
              if (isZero(R)) { e = std::move(b->lhs); ++changes; stats["algebraic_int"]++; return true; }
              break;
            case BinaryOperator::Sub:
              if (isZero(R)) { e = std::move(b->lhs); ++changes; stats["algebraic_int"]++; return true; }
              // x - x -> 0 for structurally equal subexpressions with same type
              if (b->lhs && b->rhs && b->lhs->canonical() && b->rhs->canonical() && *b->lhs->canonical() == *b->rhs->canonical()) {
                auto lt = b->lhs->type(); auto rt = b->rhs->type();
                if (lt && rt && *lt == *rt) {
                  if (*lt == TypeKind::Int) { e = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; return true; }
                  if (*lt == TypeKind::Float) { e = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; return true; }
                }
              }
              break;
            case BinaryOperator::Mul:
              if (isZero(L) || isZero(R)) { e = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; return true; }
              if (isOne(L)) { e = std::move(b->rhs); ++changes; stats["algebraic_int"]++; return true; }
              if (isOne(R)) { e = std::move(b->lhs); ++changes; stats["algebraic_int"]++; return true; }
              break;
            case BinaryOperator::Div:
              if (isOne(R)) { e = std::move(b->lhs); ++changes; stats["algebraic_int"]++; return true; }
              break;
            default: break;
          }
        }
        // Floats
        if (L->kind == NodeKind::FloatLiteral || R->kind == NodeKind::FloatLiteral) {
          switch (b->op) {
            case BinaryOperator::Add:
              if (isZero(L)) { e = std::move(b->rhs); ++changes; stats["algebraic_float"]++; return true; }
              if (isZero(R)) { e = std::move(b->lhs); ++changes; stats["algebraic_float"]++; return true; }
              break;
            case BinaryOperator::Sub:
              if (isZero(R)) { e = std::move(b->lhs); ++changes; stats["algebraic_float"]++; return true; }
              if (b->lhs && b->rhs && b->lhs->canonical() && b->rhs->canonical() && *b->lhs->canonical() == *b->rhs->canonical()) {
                auto lt = b->lhs->type(); auto rt = b->rhs->type();
                if (lt && rt && *lt == *rt && *lt == TypeKind::Float) {
                  e = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; return true;
                }
              }
              break;
            case BinaryOperator::Mul:
              if (isZero(L) || isZero(R)) { e = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; return true; }
              if (isOne(L)) { e = std::move(b->rhs); ++changes; stats["algebraic_float"]++; return true; }
              if (isOne(R)) { e = std::move(b->lhs); ++changes; stats["algebraic_float"]++; return true; }
              break;
            case BinaryOperator::Div:
              if (isOne(R)) { e = std::move(b->lhs); ++changes; stats["algebraic_float"]++; return true; }
              break;
            default: break;
          }
        }
      }
      return false;
    }
    case NodeKind::UnaryExpr: {
      auto* u = static_cast<Unary*>(e.get());
      // Double negation: -(-x) => x
      if (u->op == UnaryOperator::Neg && u->operand && u->operand->kind == NodeKind::UnaryExpr) {
        auto* inner = static_cast<Unary*>(u->operand.get());
        if (inner->op == UnaryOperator::Neg && inner->operand) {
          e = std::move(inner->operand);
          ++changes; stats["double_neg"]++;
          return true;
        }
      }
      return simplifyExpr(u->operand, changes, stats);
    }
    case NodeKind::Call: {
      auto* c = static_cast<Call*>(e.get());
      if (c->callee) simplifyExpr(c->callee, changes, stats);
      for (auto& a : c->args) simplifyExpr(a, changes, stats);
      return false;
    }
    default:
      return false;
  }
}

size_t AlgebraicSimplify::run(Module& m) {
  size_t changes = 0; stats_.clear();
  for (auto& fn : m.functions) {
    for (auto& st : fn->body) {
      if (st->kind == NodeKind::AssignStmt) {
        auto* a = static_cast<AssignStmt*>(st.get());
        simplifyExpr(a->value, changes, stats_);
      } else if (st->kind == NodeKind::ReturnStmt) {
        auto* r = static_cast<ReturnStmt*>(st.get());
        simplifyExpr(r->value, changes, stats_);
      } else if (st->kind == NodeKind::IfStmt) {
        auto* i = static_cast<IfStmt*>(st.get());
        simplifyExpr(i->cond, changes, stats_);
        for (auto& s2 : i->thenBody) {
          if (s2->kind == NodeKind::AssignStmt) {
            auto* a2 = static_cast<AssignStmt*>(s2.get()); simplifyExpr(a2->value, changes, stats_);
          } else if (s2->kind == NodeKind::ReturnStmt) {
            auto* r2 = static_cast<ReturnStmt*>(s2.get()); simplifyExpr(r2->value, changes, stats_);
          }
        }
        for (auto& s3 : i->elseBody) {
          if (s3->kind == NodeKind::AssignStmt) {
            auto* a3 = static_cast<AssignStmt*>(s3.get()); simplifyExpr(a3->value, changes, stats_);
          } else if (s3->kind == NodeKind::ReturnStmt) {
            auto* r3 = static_cast<ReturnStmt*>(s3.get()); simplifyExpr(r3->value, changes, stats_);
          }
        }
      }
    }
  }
  return changes;
}

} // namespace pycc::opt
