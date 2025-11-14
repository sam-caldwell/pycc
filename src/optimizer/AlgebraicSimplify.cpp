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

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static bool applyIntRules(Binary& bin, std::unique_ptr<Expr>& holder, size_t& changes,
                          std::unordered_map<std::string, uint64_t>& stats) {
  const Expr* left = bin.lhs.get();
  const Expr* right = bin.rhs.get();
  if (!(left && right)) { return false; }
  if (left->kind != NodeKind::IntLiteral && right->kind != NodeKind::IntLiteral) { return false; }
  switch (bin.op) {
    case BinaryOperator::Add:
      if (isZero(left)) { holder = std::move(bin.rhs); ++changes; stats["algebraic_int"]++; return true; }
      if (isZero(right)) { holder = std::move(bin.lhs); ++changes; stats["algebraic_int"]++; return true; }
      break;
    case BinaryOperator::Sub:
      if (isZero(right)) { holder = std::move(bin.lhs); ++changes; stats["algebraic_int"]++; return true; }
      break;
    case BinaryOperator::Mul:
      if (isZero(left) || isZero(right)) { holder = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; return true; }
      if (isOne(left)) { holder = std::move(bin.rhs); ++changes; stats["algebraic_int"]++; return true; }
      if (isOne(right)) { holder = std::move(bin.lhs); ++changes; stats["algebraic_int"]++; return true; }
      break;
    case BinaryOperator::Div:
      if (isOne(right)) { holder = std::move(bin.lhs); ++changes; stats["algebraic_int"]++; return true; }
      break;
    default: break;
  }
  return false;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static bool applyFloatRules(Binary& bin, std::unique_ptr<Expr>& holder, size_t& changes,
                            std::unordered_map<std::string, uint64_t>& stats) {
  const Expr* left = bin.lhs.get();
  const Expr* right = bin.rhs.get();
  if (!(left && right)) { return false; }
  if (left->kind != NodeKind::FloatLiteral && right->kind != NodeKind::FloatLiteral) { return false; }
  switch (bin.op) {
    case BinaryOperator::Add:
      if (isZero(left)) { holder = std::move(bin.rhs); ++changes; stats["algebraic_float"]++; return true; }
      if (isZero(right)) { holder = std::move(bin.lhs); ++changes; stats["algebraic_float"]++; return true; }
      break;
    case BinaryOperator::Sub:
      if (isZero(right)) { holder = std::move(bin.lhs); ++changes; stats["algebraic_float"]++; return true; }
      break;
    case BinaryOperator::Mul:
      if (isZero(left) || isZero(right)) { holder = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; return true; }
      if (isOne(left)) { holder = std::move(bin.rhs); ++changes; stats["algebraic_float"]++; return true; }
      if (isOne(right)) { holder = std::move(bin.lhs); ++changes; stats["algebraic_float"]++; return true; }
      break;
    case BinaryOperator::Div:
      if (isOne(right)) { holder = std::move(bin.lhs); ++changes; stats["algebraic_float"]++; return true; }
      break;
    default: break;
  }
  return false;
}

static bool applyCanonicalSub(Binary& bin, std::unique_ptr<Expr>& holder, size_t& changes,
                              std::unordered_map<std::string, uint64_t>& stats) {
  if (bin.op != BinaryOperator::Sub) { return false; }
  if (!(bin.lhs && bin.rhs)) { return false; }
  const auto& canLeft = bin.lhs->canonical();
  const auto& canRight = bin.rhs->canonical();
  if (!canLeft || !canRight) { return false; }
  if (!(*canLeft == *canRight)) { return false; }
  auto leftType = bin.lhs->type(); auto rightType = bin.rhs->type();
  if (!(leftType && rightType) || !(*leftType == *rightType)) { return false; }
  if (*leftType == TypeKind::Int) { holder = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; return true; }
  if (*leftType == TypeKind::Float) { holder = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; return true; }
  return false;
}

// Forward decl since simplifyBinary dispatches recursively
static bool simplifyExpr(std::unique_ptr<Expr>& expr, size_t& changes,
                         std::unordered_map<std::string, uint64_t>& stats);

// NOLINTNEXTLINE(readability-function-size)
static bool simplifyBinary(std::unique_ptr<Expr>& expr, size_t& changes,
                           std::unordered_map<std::string, uint64_t>& stats) {
  auto* bin = static_cast<Binary*>(expr.get());
  simplifyExpr(bin->lhs, changes, stats);
  simplifyExpr(bin->rhs, changes, stats);
  std::unique_ptr<Expr> out;
  if (applyIntRules(*bin, out, changes, stats)) { expr = std::move(out); return true; }
  if (applyFloatRules(*bin, out, changes, stats)) { expr = std::move(out); return true; }
  if (applyCanonicalSub(*bin, out, changes, stats)) { expr = std::move(out); return true; }
  return false;
}

// NOLINTNEXTLINE(readability-function-size)
static bool simplifyExpr(std::unique_ptr<Expr>& expr, size_t& changes, std::unordered_map<std::string,uint64_t>& stats) {
  if (!expr) { return false; }
  switch (expr->kind) {
    case NodeKind::BinaryExpr:
      return simplifyBinary(expr, changes, stats);
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

// NOLINTNEXTLINE(readability-function-size)
static void simplifyBlock(std::vector<std::unique_ptr<Stmt>>& body, size_t& changes,
                          std::unordered_map<std::string, uint64_t>& stats) {
  for (auto& stmt : body) {
    switch (stmt->kind) {
      case NodeKind::AssignStmt: {
        auto* assign = static_cast<AssignStmt*>(stmt.get());
        simplifyExpr(assign->value, changes, stats);
        break;
      }
      case NodeKind::ReturnStmt: {
        auto* ret = static_cast<ReturnStmt*>(stmt.get());
        simplifyExpr(ret->value, changes, stats);
        break;
      }
      case NodeKind::IfStmt: {
        auto* ifStmt = static_cast<IfStmt*>(stmt.get());
        simplifyExpr(ifStmt->cond, changes, stats);
        simplifyBlock(ifStmt->thenBody, changes, stats);
        simplifyBlock(ifStmt->elseBody, changes, stats);
        break;
      }
      default: break;
    }
  }
}

size_t AlgebraicSimplify::run(Module& module) {
  size_t changes = 0;
  stats_.clear();
  for (auto& func : module.functions) { simplifyBlock(func->body, changes, stats_); }
  return changes;
}

} // namespace pycc::opt
