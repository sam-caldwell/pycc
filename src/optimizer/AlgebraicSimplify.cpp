/***
 * Name: pycc::opt::AlgebraicSimplify (impl)
 */
#include "optimizer/AlgebraicSimplify.h"
#include "ast/BinaryOperator.h"
#include "ast/BoolLiteral.h"
#include "ast/Expr.h"
#include "ast/FloatLiteral.h"
#include "ast/IntLiteral.h"
#include "ast/NodeKind.h"
#include "ast/Stmt.h"
#include "ast/StringLiteral.h"
#include "ast/TypeKind.h"
#include "ast/UnaryOperator.h"
#include "ast/VisitorBase.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pycc::opt {
using namespace pycc::ast;

// Literal classification helpers at file scope
static inline bool isZero(const Expr* expr) {
  if (expr == nullptr) { return false; }
  if (expr->kind == NodeKind::IntLiteral) { return static_cast<const IntLiteral*>(expr)->value == 0; }
  if (expr->kind == NodeKind::FloatLiteral) { return static_cast<const FloatLiteral*>(expr)->value == 0.0; }
  return false;
}
static inline bool isOne(const Expr* expr) {
  if (expr == nullptr) { return false; }
  if (expr->kind == NodeKind::IntLiteral) { return static_cast<const IntLiteral*>(expr)->value == 1; }
  if (expr->kind == NodeKind::FloatLiteral) { return static_cast<const FloatLiteral*>(expr)->value == 1.0; }
  return false;
}

namespace {

struct SimplifyVisitor : public ast::VisitorBase {
  std::unordered_map<std::string, uint64_t>& stats;
  size_t& changes;
  std::unique_ptr<Expr>* slot{nullptr};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};

  explicit SimplifyVisitor(std::unordered_map<std::string, uint64_t>& statsMap, size_t& changeCount)
      : stats(statsMap), changes(changeCount) {}

  void rewrite(std::unique_ptr<Expr>& expr) {
    if (!expr) { return; }
    slot = &expr;
    expr->accept(*this);
  }

  void touch(std::unique_ptr<Stmt>& stmt) {
    if (!stmt) { return; }
    stmtSlot = &stmt;
    stmt->accept(*this);
  }
  void walkBlock(std::vector<std::unique_ptr<Stmt>>& body) {
    for (auto& stmt : body) { touch(stmt); }
  }

  // Expr visitors
  // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
  void visit(const Binary& binary) override {
    (void)binary;
    auto* bin = static_cast<Binary*>(slot->get());
    auto* exprSlot = slot;
    rewrite(bin->lhs);
    rewrite(bin->rhs);

    std::unique_ptr<Expr> replacement;
    const Expr* left = bin->lhs.get();
    const Expr* right = bin->rhs.get();
    if (left && right) {
      // Int rules
      if (left->kind == NodeKind::IntLiteral || right->kind == NodeKind::IntLiteral) {
        switch (bin->op) {
          case BinaryOperator::Add:
            if (isZero(left)) { replacement = std::move(bin->rhs); } else if (isZero(right)) { replacement = std::move(bin->lhs); }
            if (replacement) { ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Sub:
            if (isZero(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Mul:
            if (isZero(left) || isZero(right)) { replacement = std::make_unique<IntLiteral>(0); }
            else if (isOne(left)) { replacement = std::move(bin->rhs); }
            else if (isOne(right)) { replacement = std::move(bin->lhs); }
            if (replacement) { ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Div:
            if (isOne(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          default: break;
        }
      }
      // Float rules
      if (left->kind == NodeKind::FloatLiteral || right->kind == NodeKind::FloatLiteral) {
        switch (bin->op) {
          case BinaryOperator::Add:
            if (isZero(left)) { replacement = std::move(bin->rhs); } else if (isZero(right)) { replacement = std::move(bin->lhs); }
            if (replacement) { ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Sub:
            if (isZero(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Mul:
            if (isZero(left) || isZero(right)) { replacement = std::make_unique<FloatLiteral>(0.0); }
            else if (isOne(left)) { replacement = std::move(bin->rhs); }
            else if (isOne(right)) { replacement = std::move(bin->lhs); }
            if (replacement) { ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Div:
            if (isOne(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            break;
          default: break;
        }
      }
      // Canonical x - x -> 0 (typed)
      if (bin->op == BinaryOperator::Sub) {
        const auto& canonLhs = bin->lhs->canonical();
        const auto& canonRhs = bin->rhs->canonical();
        if (canonLhs && canonRhs && *canonLhs == *canonRhs) {
          auto typeL = bin->lhs->type(); auto typeR = bin->rhs->type();
          if (typeL && typeR && *typeL == *typeR) {
            if (*typeL == TypeKind::Int) { replacement = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; slot = exprSlot; *slot = std::move(replacement); return; }
            if (*typeL == TypeKind::Float) { replacement = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; slot = exprSlot; *slot = std::move(replacement); return; }
          }
        }
      }
    }
  }

  void visit(const Unary& unary) override {
    (void)unary;
    auto* unaryNode = static_cast<Unary*>(slot->get());
    auto* exprSlot = slot;
    if (unaryNode->op == UnaryOperator::Neg && unaryNode->operand && unaryNode->operand->kind == NodeKind::UnaryExpr) {
      auto* inner = static_cast<Unary*>(unaryNode->operand.get());
      if (inner->op == UnaryOperator::Neg && inner->operand) {
        slot = exprSlot; *slot = std::move(inner->operand); ++changes; stats["double_neg"]++;
        return;
      }
    }
    rewrite(unaryNode->operand);
  }

  void visit(const Call& callExpr) override {
    (void)callExpr;
    auto* call = static_cast<Call*>(slot->get());
    if (call->callee) { rewrite(call->callee); }
    for (auto& arg : call->args) { rewrite(arg); }
  }

  // Stmt visitors
  void visit(const AssignStmt& assignStmt) override {
    (void)assignStmt;
    auto* assignNode = static_cast<AssignStmt*>(stmtSlot->get());
    rewrite(assignNode->value);
  }
  void visit(const ReturnStmt& retStmt) override {
    (void)retStmt;
    auto* retNode = static_cast<ReturnStmt*>(stmtSlot->get());
    rewrite(retNode->value);
  }
  void visit(const IfStmt& ifStmt) override {
    (void)ifStmt;
    auto* ifNode = static_cast<IfStmt*>(stmtSlot->get());
    rewrite(ifNode->cond);
    walkBlock(ifNode->thenBody);
    walkBlock(ifNode->elseBody);
  }

  // No-ops for other nodes
  void visit(const Module& module) override { (void)module; }
  void visit(const FunctionDef& functionDef) override { (void)functionDef; }
  void visit(const ExprStmt& exprStmt) override { (void)exprStmt; }
  void visit(const IntLiteral& intLiteral) override { (void)intLiteral; }
  void visit(const BoolLiteral& boolLiteral) override { (void)boolLiteral; }
  void visit(const FloatLiteral& floatLiteral) override { (void)floatLiteral; }
  void visit(const StringLiteral& stringLiteral) override { (void)stringLiteral; }
  void visit(const NoneLiteral& noneLiteral) override { (void)noneLiteral; }
  void visit(const Name& name) override { (void)name; }
  void visit(const TupleLiteral& tupleLiteral) override { (void)tupleLiteral; }
  void visit(const ListLiteral& listLiteral) override { (void)listLiteral; }
  void visit(const ObjectLiteral& objectLiteral) override { (void)objectLiteral; }
};

} // namespace


size_t AlgebraicSimplify::run(Module& module) {
  size_t changes = 0;
  stats_.clear();
  SimplifyVisitor vis(stats_, changes);
  for (auto& func : module.functions) { vis.walkBlock(func->body); }
  return changes;
}

} // namespace pycc::opt
