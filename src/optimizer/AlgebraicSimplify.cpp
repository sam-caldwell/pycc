/***
 * Name: pycc::opt::AlgebraicSimplify (impl)
 */
#include "optimizer/AlgebraicSimplify.h"
#include "ast/VisitorBase.h"
#include <memory>

namespace pycc::opt {
using namespace pycc::ast;

namespace {
inline bool isZero(const Expr* expr) {
  if (expr == nullptr) { return false; }
  if (expr->kind == NodeKind::IntLiteral) { return static_cast<const IntLiteral*>(expr)->value == 0; }
  if (expr->kind == NodeKind::FloatLiteral) { return static_cast<const FloatLiteral*>(expr)->value == 0.0; }
  return false;
}
inline bool isOne(const Expr* expr) {
  if (expr == nullptr) { return false; }
  if (expr->kind == NodeKind::IntLiteral) { return static_cast<const IntLiteral*>(expr)->value == 1; }
  if (expr->kind == NodeKind::FloatLiteral) { return static_cast<const FloatLiteral*>(expr)->value == 1.0; }
  return false;
}

struct SimplifyVisitor : public ast::VisitorBase {
  std::unordered_map<std::string, uint64_t>& stats;
  size_t& changes;
  std::unique_ptr<Expr>* slot{nullptr};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};

  explicit SimplifyVisitor(std::unordered_map<std::string, uint64_t>& s, size_t& c)
      : stats(s), changes(c) {}

  void rewrite(std::unique_ptr<Expr>& e) {
    if (!e) { return; }
    slot = &e;
    e->accept(*this);
  }

  void touch(std::unique_ptr<Stmt>& s) {
    if (!s) { return; }
    stmtSlot = &s;
    s->accept(*this);
  }
  void walkBlock(std::vector<std::unique_ptr<Stmt>>& body) {
    for (auto& st : body) { touch(st); }
  }

  // Expr visitors
  // NOLINTNEXTLINE(readability-function-size,readability-function-cognitive-complexity)
  void visit(const Binary&) override {
    auto* bin = static_cast<Binary*>(slot->get());
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
            if (replacement) { ++changes; stats["algebraic_int"]++; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Sub:
            if (isZero(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Mul:
            if (isZero(left) || isZero(right)) { replacement = std::make_unique<IntLiteral>(0); }
            else if (isOne(left)) { replacement = std::move(bin->rhs); }
            else if (isOne(right)) { replacement = std::move(bin->lhs); }
            if (replacement) { ++changes; stats["algebraic_int"]++; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Div:
            if (isOne(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_int"]++; *slot = std::move(replacement); return; }
            break;
          default: break;
        }
      }
      // Float rules
      if (left->kind == NodeKind::FloatLiteral || right->kind == NodeKind::FloatLiteral) {
        switch (bin->op) {
          case BinaryOperator::Add:
            if (isZero(left)) { replacement = std::move(bin->rhs); } else if (isZero(right)) { replacement = std::move(bin->lhs); }
            if (replacement) { ++changes; stats["algebraic_float"]++; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Sub:
            if (isZero(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Mul:
            if (isZero(left) || isZero(right)) { replacement = std::make_unique<FloatLiteral>(0.0); }
            else if (isOne(left)) { replacement = std::move(bin->rhs); }
            else if (isOne(right)) { replacement = std::move(bin->lhs); }
            if (replacement) { ++changes; stats["algebraic_float"]++; *slot = std::move(replacement); return; }
            break;
          case BinaryOperator::Div:
            if (isOne(right)) { replacement = std::move(bin->lhs); ++changes; stats["algebraic_float"]++; *slot = std::move(replacement); return; }
            break;
          default: break;
        }
      }
      // Canonical x - x -> 0 (typed)
      if (bin->op == BinaryOperator::Sub) {
        const auto& cl = bin->lhs->canonical();
        const auto& cr = bin->rhs->canonical();
        if (cl && cr && *cl == *cr) {
          auto tl = bin->lhs->type(); auto tr = bin->rhs->type();
          if (tl && tr && *tl == *tr) {
            if (*tl == TypeKind::Int) { replacement = std::make_unique<IntLiteral>(0); ++changes; stats["algebraic_int"]++; *slot = std::move(replacement); return; }
            if (*tl == TypeKind::Float) { replacement = std::make_unique<FloatLiteral>(0.0); ++changes; stats["algebraic_float"]++; *slot = std::move(replacement); return; }
          }
        }
      }
    }
  }

  void visit(const Unary&) override {
    auto* un = static_cast<Unary*>(slot->get());
    if (un->op == UnaryOperator::Neg && un->operand && un->operand->kind == NodeKind::UnaryExpr) {
      auto* inner = static_cast<Unary*>(un->operand.get());
      if (inner->op == UnaryOperator::Neg && inner->operand) {
        *slot = std::move(inner->operand); ++changes; stats["double_neg"]++;
        return;
      }
    }
    rewrite(un->operand);
  }

  void visit(const Call&) override {
    auto* call = static_cast<Call*>(slot->get());
    if (call->callee) { rewrite(call->callee); }
    for (auto& arg : call->args) { rewrite(arg); }
  }

  // Stmt visitors
  void visit(const AssignStmt&) override {
    auto* as = static_cast<AssignStmt*>(stmtSlot->get());
    rewrite(as->value);
  }
  void visit(const ReturnStmt&) override {
    auto* rs = static_cast<ReturnStmt*>(stmtSlot->get());
    rewrite(rs->value);
  }
  void visit(const IfStmt&) override {
    auto* ifs = static_cast<IfStmt*>(stmtSlot->get());
    rewrite(ifs->cond);
    walkBlock(ifs->thenBody);
    walkBlock(ifs->elseBody);
  }

  // No-ops for other nodes
  void visit(const Module&) override {}
  void visit(const FunctionDef&) override {}
  void visit(const ExprStmt&) override {}
  void visit(const IntLiteral&) override {}
  void visit(const BoolLiteral&) override {}
  void visit(const FloatLiteral&) override {}
  void visit(const StringLiteral&) override {}
  void visit(const NoneLiteral&) override {}
  void visit(const Name&) override {}
  void visit(const TupleLiteral&) override {}
  void visit(const ListLiteral&) override {}
  void visit(const ObjectLiteral&) override {}
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
