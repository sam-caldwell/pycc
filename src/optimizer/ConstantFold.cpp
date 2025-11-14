/***
 * Name: pycc::opt::ConstantFold (impl)
 * Purpose: Fold constant expressions via AST mutation using visitors.
 */
#include "optimizer/ConstantFold.h"
#include <memory>

namespace pycc::opt {

using namespace pycc::ast;

template <typename T>
inline bool isLit(const Expr* /*e*/) { return false; }

template <>
bool isLit<IntLiteral>(const Expr* expr) { return expr != nullptr && expr->kind == NodeKind::IntLiteral; }
// Silence unused function warning for BoolLiteral specialization by using it
template <>
bool isLit<FloatLiteral>(const Expr* expr) { return expr != nullptr && expr->kind == NodeKind::FloatLiteral; }

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool foldExpr(std::unique_ptr<Expr>& expr, size_t& changes, std::unordered_map<std::string,uint64_t>& stats);

static bool foldUnary(Unary& unary, std::unique_ptr<Expr>& holder, size_t& changes, std::unordered_map<std::string,uint64_t>& stats) {
  if (unary.operand) { foldExpr(unary.operand, changes, stats); }
  if (unary.op == UnaryOperator::Neg) {
    if (isLit<IntLiteral>(unary.operand.get())) {
      auto* lit = static_cast<IntLiteral*>(unary.operand.get());
      holder = std::make_unique<IntLiteral>(-lit->value);
      ++changes; stats["unary"]++; return true;
    }
    if (isLit<FloatLiteral>(unary.operand.get())) {
      auto* lit = static_cast<FloatLiteral*>(unary.operand.get());
      holder = std::make_unique<FloatLiteral>(-lit->value);
      ++changes; stats["unary"]++; return true;
    }
  }
  return false;
}

// NOLINTNEXTLINE(readability-function-size)
static bool foldBinaryInt(Binary& binary, std::unique_ptr<Expr>& holder, size_t& changes,
                          std::unordered_map<std::string, uint64_t>& stats) {
  auto* left = binary.lhs.get();
  auto* right = binary.rhs.get();
  if (!(isLit<IntLiteral>(left) && isLit<IntLiteral>(right))) { return false; }
  auto* leftInt = static_cast<IntLiteral*>(left);
  auto* rightInt = static_cast<IntLiteral*>(right);
  long long resultInt = 0;
  switch (binary.op) {
    case BinaryOperator::Add: resultInt = leftInt->value + rightInt->value; break;
    case BinaryOperator::Sub: resultInt = leftInt->value - rightInt->value; break;
    case BinaryOperator::Mul: resultInt = leftInt->value * rightInt->value; break;
    case BinaryOperator::Div: {
      if (rightInt->value == 0) { return false; }
      resultInt = leftInt->value / rightInt->value;
      break;
    }
    case BinaryOperator::Mod: {
      if (rightInt->value == 0) { return false; }
      resultInt = leftInt->value % rightInt->value;
      break;
    }
    case BinaryOperator::Eq: holder = std::make_unique<BoolLiteral>(leftInt->value == rightInt->value); ++changes; stats["compare_int"]++; return true;
    case BinaryOperator::Ne: holder = std::make_unique<BoolLiteral>(leftInt->value != rightInt->value); ++changes; stats["compare_int"]++; return true;
    case BinaryOperator::Lt: holder = std::make_unique<BoolLiteral>(leftInt->value < rightInt->value); ++changes; stats["compare_int"]++; return true;
    case BinaryOperator::Le: holder = std::make_unique<BoolLiteral>(leftInt->value <= rightInt->value); ++changes; stats["compare_int"]++; return true;
    case BinaryOperator::Gt: holder = std::make_unique<BoolLiteral>(leftInt->value > rightInt->value); ++changes; stats["compare_int"]++; return true;
    case BinaryOperator::Ge: holder = std::make_unique<BoolLiteral>(leftInt->value >= rightInt->value); ++changes; stats["compare_int"]++; return true;
    default: return false;
  }
  holder = std::make_unique<IntLiteral>(resultInt);
  ++changes; stats["binary_int"]++; return true;
}

// NOLINTNEXTLINE(readability-function-size)
static bool foldBinaryFloat(Binary& binary, std::unique_ptr<Expr>& holder, size_t& changes,
                            std::unordered_map<std::string, uint64_t>& stats) {
  auto* left = binary.lhs.get();
  auto* right = binary.rhs.get();
  if (!(isLit<FloatLiteral>(left) && isLit<FloatLiteral>(right))) { return false; }
  auto* leftFloat = static_cast<FloatLiteral*>(left);
  auto* rightFloat = static_cast<FloatLiteral*>(right);
  double result = 0.0;
  switch (binary.op) {
    case BinaryOperator::Add: result = leftFloat->value + rightFloat->value; break;
    case BinaryOperator::Sub: result = leftFloat->value - rightFloat->value; break;
    case BinaryOperator::Mul: result = leftFloat->value * rightFloat->value; break;
    case BinaryOperator::Div: {
      if (rightFloat->value == 0.0) { return false; }
      result = leftFloat->value / rightFloat->value;
      break;
    }
    case BinaryOperator::Eq: holder = std::make_unique<BoolLiteral>(leftFloat->value == rightFloat->value); ++changes; stats["compare_float"]++; return true;
    case BinaryOperator::Ne: holder = std::make_unique<BoolLiteral>(leftFloat->value != rightFloat->value); ++changes; stats["compare_float"]++; return true;
    case BinaryOperator::Lt: holder = std::make_unique<BoolLiteral>(leftFloat->value < rightFloat->value); ++changes; stats["compare_float"]++; return true;
    case BinaryOperator::Le: holder = std::make_unique<BoolLiteral>(leftFloat->value <= rightFloat->value); ++changes; stats["compare_float"]++; return true;
    case BinaryOperator::Gt: holder = std::make_unique<BoolLiteral>(leftFloat->value > rightFloat->value); ++changes; stats["compare_float"]++; return true;
    case BinaryOperator::Ge: holder = std::make_unique<BoolLiteral>(leftFloat->value >= rightFloat->value); ++changes; stats["compare_float"]++; return true;
    default: return false;
  }
  holder = std::make_unique<FloatLiteral>(result);
  ++changes; stats["binary_float"]++; return true;
}

static bool foldBinary(Binary& binary, std::unique_ptr<Expr>& holder, size_t& changes, std::unordered_map<std::string,uint64_t>& stats) {
  if (binary.lhs) { foldExpr(binary.lhs, changes, stats); }
  if (binary.rhs) { foldExpr(binary.rhs, changes, stats); }
  if (foldBinaryInt(binary, holder, changes, stats)) { return true; }
  if (foldBinaryFloat(binary, holder, changes, stats)) { return true; }
  return false;
}

// NOLINTNEXTLINE(readability-function-size)
static bool foldExpr(std::unique_ptr<Expr>& expr, size_t& changes, std::unordered_map<std::string,uint64_t>& stats) {
  if (!expr) { return false; }
  switch (expr->kind) {
    case NodeKind::UnaryExpr: {
      auto* unExpr = static_cast<Unary*>(expr.get());
      std::unique_ptr<Expr> rep;
      if (foldUnary(*unExpr, rep, changes, stats)) {
        rep->file = expr->file; rep->line = expr->line; rep->col = expr->col; expr = std::move(rep); return true;
      }
      return false;
    }
    case NodeKind::BinaryExpr: {
      auto* binExpr = static_cast<Binary*>(expr.get());
      std::unique_ptr<Expr> rep;
      if (foldBinary(*binExpr, rep, changes, stats)) {
        rep->file = expr->file; rep->line = expr->line; rep->col = expr->col; expr = std::move(rep); return true;
      }
      return false;
    }
    case NodeKind::Call: {
      auto* callExpr = static_cast<Call*>(expr.get());
      if (callExpr->callee) { foldExpr(callExpr->callee, changes, stats); }
      for (auto& argExpr : callExpr->args) { foldExpr(argExpr, changes, stats); }
      return false;
    }
    default:
      return false;
  }
}

// end local helpers

// NOLINTNEXTLINE(readability-function-size)
static void foldBlock(std::vector<std::unique_ptr<Stmt>>& body, size_t& changes,
                      std::unordered_map<std::string, uint64_t>& stats) {
  for (auto& stmt : body) {
    switch (stmt->kind) {
      case NodeKind::AssignStmt: {
        auto* assignStmt = static_cast<AssignStmt*>(stmt.get());
        foldExpr(assignStmt->value, changes, stats);
        break;
      }
      case NodeKind::ReturnStmt: {
        auto* retStmt = static_cast<ReturnStmt*>(stmt.get());
        foldExpr(retStmt->value, changes, stats);
        break;
      }
      case NodeKind::IfStmt: {
        auto* ifStmt = static_cast<IfStmt*>(stmt.get());
        foldExpr(ifStmt->cond, changes, stats);
        foldBlock(ifStmt->thenBody, changes, stats);
        foldBlock(ifStmt->elseBody, changes, stats);
        break;
      }
      default: break;
    }
  }
}

size_t ConstantFold::run(Module& module) {
  size_t changes = 0;
  stats_.clear();
  for (auto& func : module.functions) { foldBlock(func->body, changes, stats_); }
  return changes;
}

} // namespace pycc::opt
