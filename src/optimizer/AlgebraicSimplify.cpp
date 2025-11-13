/***
 * Name: pycc::opt::AlgebraicSimplify (impl)
 */
#include "optimizer/AlgebraicSimplify.h"
#include <memory>

namespace pycc::opt {
using namespace pycc::ast;

static bool isZero(const Expr* expr) {
  if (expr == nullptr) { return false; }
  if (expr->kind == NodeKind::IntLiteral) { return static_cast<const IntLiteral*>(expr)->value == 0; }
  if (expr->kind == NodeKind::FloatLiteral) { return static_cast<const FloatLiteral*>(expr)->value == 0.0; }
  return false;
}
static bool isOne(const Expr* expr) {
  if (expr == nullptr) { return false; }
  if (expr->kind == NodeKind::IntLiteral) { return static_cast<const IntLiteral*>(expr)->value == 1; }
  if (expr->kind == NodeKind::FloatLiteral) { return static_cast<const FloatLiteral*>(expr)->value == 1.0; }
  return false;
}

// NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
static bool simplifyExpr(std::unique_ptr<Expr>& expr, size_t& changes, std::unordered_map<std::string,uint64_t>& stats) {
  if (!expr) { return false; }
  switch (expr->kind) {
    case NodeKind::BinaryExpr: {
      auto* bin = static_cast<Binary*>(expr.get());
      simplifyExpr(bin->lhs, changes, stats);
      simplifyExpr(bin->rhs, changes, stats);
      const Expr* left = bin->lhs.get(); const Expr* right = bin->rhs.get();
      if ((left != nullptr) && (right != nullptr)) {
        // Integers
        if (left->kind == NodeKind::IntLiteral || right->kind == NodeKind::IntLiteral) {
          switch (bin->op) {
            case BinaryOperator::Add:
              if (isZero(left)) { expr = std::move(bin->rhs); ++changes; stats["algebraic_int"]++; return true; }
              if (isZero(right)) { expr = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; return true; }
              break;
            case BinaryOperator::Sub:
              if (isZero(right)) { expr = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; return true; }
              // x - x -> 0 for structurally equal subexpressions with same type
              if (bin->lhs && bin->rhs && bin->lhs->canonical() && bin->rhs->canonical() && *bin->lhs->canonical() == *bin->rhs->canonical()) {
                auto leftType = bin->lhs->type(); auto rightType = bin->rhs->type();
                if (leftType && rightType && *leftType == *rightType) {
                  if (*leftType == TypeKind::Int) { expr = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; return true; }
                  if (*leftType == TypeKind::Float) { expr = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; return true; }
                }
              }
              break;
            case BinaryOperator::Mul:
              if (isZero(left) || isZero(right)) { expr = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; return true; }
              if (isOne(left)) { expr = std::move(bin->rhs); ++changes; stats["algebraic_int"]++; return true; }
              if (isOne(right)) { expr = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; return true; }
              break;
            case BinaryOperator::Div:
              if (isOne(right)) { expr = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; return true; }
              break;
            default: break;
          }
        }
        // Floats
        if (left->kind == NodeKind::FloatLiteral || right->kind == NodeKind::FloatLiteral) {
          switch (bin->op) {
            case BinaryOperator::Add:
              if (isZero(left)) { expr = std::move(bin->rhs); ++changes; stats["algebraic_float"]++; return true; }
              if (isZero(right)) { expr = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; return true; }
              break;
            case BinaryOperator::Sub:
              if (isZero(right)) { expr = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; return true; }
              if (bin->lhs && bin->rhs && bin->lhs->canonical() && bin->rhs->canonical() && *bin->lhs->canonical() == *bin->rhs->canonical()) {
                auto leftType = bin->lhs->type(); auto rightType = bin->rhs->type();
                if (leftType && rightType && *leftType == *rightType && *leftType == TypeKind::Float) {
                  expr = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; return true;
                }
              }
              break;
            case BinaryOperator::Mul:
              if (isZero(left) || isZero(right)) { expr = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; return true; }
              if (isOne(left)) { expr = std::move(bin->rhs); ++changes; stats["algebraic_float"]++; return true; }
              if (isOne(right)) { expr = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; return true; }
              break;
            case BinaryOperator::Div:
              if (isOne(right)) { expr = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; return true; }
              break;
            default: break;
          }
        }
        // Generic canonical equality: x - x -> 0 for typed int/float even when no literals are present
        if (bin->op == BinaryOperator::Sub) {
          if (bin->lhs && bin->rhs && bin->lhs->canonical() && bin->rhs->canonical() && *bin->lhs->canonical() == *bin->rhs->canonical()) {
            auto leftType = bin->lhs->type(); auto rightType = bin->rhs->type();
            if (leftType && rightType && *leftType == *rightType) {
              if (*leftType == TypeKind::Int) { expr = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; return true; }
              if (*leftType == TypeKind::Float) { expr = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; return true; }
            }
          }
        }
      }
      return false;
    }
    case NodeKind::UnaryExpr: {
      auto* unaryExpr = static_cast<Unary*>(expr.get());
      // Double negation: -(-x) => x
      if (unaryExpr->op == UnaryOperator::Neg && unaryExpr->operand && unaryExpr->operand->kind == NodeKind::UnaryExpr) {
        auto* inner = static_cast<Unary*>(unaryExpr->operand.get());
        if (inner->op == UnaryOperator::Neg && inner->operand) {
          expr = std::move(inner->operand);
          ++changes; stats["double_neg"]++;
          return true;
        }
      }
      return simplifyExpr(unaryExpr->operand, changes, stats);
    }
    case NodeKind::Call: {
      auto* call = static_cast<Call*>(expr.get());
      if (call->callee) { simplifyExpr(call->callee, changes, stats); }
      for (auto& arg : call->args) { simplifyExpr(arg, changes, stats); }
      return false;
    }
    default:
      return false;
  }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
size_t AlgebraicSimplify::run(Module& module) {
  size_t changes = 0; stats_.clear();
  for (auto& func : module.functions) {
    for (auto& stmt : func->body) {
      if (stmt->kind == NodeKind::AssignStmt) {
        auto* assign = static_cast<AssignStmt*>(stmt.get());
        simplifyExpr(assign->value, changes, stats_);
      } else if (stmt->kind == NodeKind::ReturnStmt) {
        auto* ret = static_cast<ReturnStmt*>(stmt.get());
        simplifyExpr(ret->value, changes, stats_);
      } else if (stmt->kind == NodeKind::IfStmt) {
        auto* ifStmt = static_cast<IfStmt*>(stmt.get());
        simplifyExpr(ifStmt->cond, changes, stats_);
        for (auto& thenStmt : ifStmt->thenBody) {
          if (thenStmt->kind == NodeKind::AssignStmt) {
            auto* assignThen = static_cast<AssignStmt*>(thenStmt.get());
            simplifyExpr(assignThen->value, changes, stats_);
          } else if (thenStmt->kind == NodeKind::ReturnStmt) {
            auto* retThen = static_cast<ReturnStmt*>(thenStmt.get());
            simplifyExpr(retThen->value, changes, stats_);
          }
        }
        for (auto& elseStmt : ifStmt->elseBody) {
          if (elseStmt->kind == NodeKind::AssignStmt) {
            auto* assignElse = static_cast<AssignStmt*>(elseStmt.get());
            simplifyExpr(assignElse->value, changes, stats_);
          } else if (elseStmt->kind == NodeKind::ReturnStmt) {
            auto* retElse = static_cast<ReturnStmt*>(elseStmt.get());
            simplifyExpr(retElse->value, changes, stats_);
          }
        }
      }
    }
  }
  return changes;
}

} // namespace pycc::opt
