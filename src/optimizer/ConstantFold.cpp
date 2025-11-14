/***
 * Name: pycc::opt::ConstantFold (impl)
 * Purpose: Fold constant expressions via AST mutation using a visitor.
 */
#include "optimizer/ConstantFold.h"
#include "ast/VisitorBase.h"
#include <memory>

namespace pycc::opt {

using namespace pycc::ast;

namespace {
template <typename T>
inline bool isLit(const Expr* /*e*/) { return false; }
template <>
bool isLit<IntLiteral>(const Expr* expr) { return expr != nullptr && expr->kind == NodeKind::IntLiteral; }
template <>
bool isLit<FloatLiteral>(const Expr* expr) { return expr != nullptr && expr->kind == NodeKind::FloatLiteral; }

struct FoldVisitor : public ast::VisitorBase {
  std::unordered_map<std::string, uint64_t>& stats;
  size_t& changes;
  std::unique_ptr<Expr>* slot{nullptr};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};

  FoldVisitor(std::unordered_map<std::string, uint64_t>& s, size_t& c) : stats(s), changes(c) {}

  static void assignLoc(Expr& dst, const Expr& src) { dst.file = src.file; dst.line = src.line; dst.col = src.col; }

  void rewrite(std::unique_ptr<Expr>& e) { if (!e) { return; } slot = &e; e->accept(*this); }

  void touch(std::unique_ptr<Stmt>& s) { if (!s) { return; } stmtSlot = &s; s->accept(*this); }
  void walkBlock(std::vector<std::unique_ptr<Stmt>>& body) { for (auto& st : body) { touch(st); } }

  void visit(const Unary&) override {
    auto* un = static_cast<Unary*>(slot->get());
    if (un->op == UnaryOperator::Neg && un->operand) {
      if (isLit<IntLiteral>(un->operand.get())) {
        auto* lit = static_cast<IntLiteral*>(un->operand.get());
        auto rep = std::make_unique<IntLiteral>(-lit->value);
        assignLoc(*rep, *un); *slot = std::move(rep); ++changes; stats["unary"]++; return;
      }
      if (isLit<FloatLiteral>(un->operand.get())) {
        auto* lit = static_cast<FloatLiteral*>(un->operand.get());
        auto rep = std::make_unique<FloatLiteral>(-lit->value);
        assignLoc(*rep, *un); *slot = std::move(rep); ++changes; stats["unary"]++; return;
      }
    }
    rewrite(un->operand);
  }

  bool foldIntArith(Binary& bin, long long a, long long b) {
    long long res = 0;
    switch (bin.op) {
      case BinaryOperator::Add: res = a + b; stats["binary_int"]++; break;
      case BinaryOperator::Sub: res = a - b; stats["binary_int"]++; break;
      case BinaryOperator::Mul: res = a * b; stats["binary_int"]++; break;
      case BinaryOperator::Div: if (b == 0) { return false; } res = a / b; stats["binary_int"]++; break;
      case BinaryOperator::Mod: if (b == 0) { return false; } res = a % b; stats["binary_int"]++; break;
      default: return false;
    }
    auto rep = std::make_unique<IntLiteral>(res); assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }
  bool foldIntCmp(Binary& bin, long long a, long long b) {
    std::unique_ptr<Expr> rep;
    switch (bin.op) {
      case BinaryOperator::Eq: rep = std::make_unique<BoolLiteral>(a == b); stats["compare_int"]++; break;
      case BinaryOperator::Ne: rep = std::make_unique<BoolLiteral>(a != b); stats["compare_int"]++; break;
      case BinaryOperator::Lt: rep = std::make_unique<BoolLiteral>(a < b); stats["compare_int"]++; break;
      case BinaryOperator::Le: rep = std::make_unique<BoolLiteral>(a <= b); stats["compare_int"]++; break;
      case BinaryOperator::Gt: rep = std::make_unique<BoolLiteral>(a > b); stats["compare_int"]++; break;
      case BinaryOperator::Ge: rep = std::make_unique<BoolLiteral>(a >= b); stats["compare_int"]++; break;
      default: return false;
    }
    assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }
  bool foldFloatArith(Binary& bin, double a, double b) {
    double res = 0.0;
    switch (bin.op) {
      case BinaryOperator::Add: res = a + b; stats["binary_float"]++; break;
      case BinaryOperator::Sub: res = a - b; stats["binary_float"]++; break;
      case BinaryOperator::Mul: res = a * b; stats["binary_float"]++; break;
      case BinaryOperator::Div: if (b == 0.0) { return false; } res = a / b; stats["binary_float"]++; break;
      default: return false;
    }
    auto rep = std::make_unique<FloatLiteral>(res); assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }
  bool foldFloatCmp(Binary& bin, double a, double b) {
    std::unique_ptr<Expr> rep;
    switch (bin.op) {
      case BinaryOperator::Eq: rep = std::make_unique<BoolLiteral>(a == b); stats["compare_float"]++; break;
      case BinaryOperator::Ne: rep = std::make_unique<BoolLiteral>(a != b); stats["compare_float"]++; break;
      case BinaryOperator::Lt: rep = std::make_unique<BoolLiteral>(a < b); stats["compare_float"]++; break;
      case BinaryOperator::Le: rep = std::make_unique<BoolLiteral>(a <= b); stats["compare_float"]++; break;
      case BinaryOperator::Gt: rep = std::make_unique<BoolLiteral>(a > b); stats["compare_float"]++; break;
      case BinaryOperator::Ge: rep = std::make_unique<BoolLiteral>(a >= b); stats["compare_float"]++; break;
      default: return false;
    }
    assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }

  // NOLINTNEXTLINE(readability-function-cognitive-complexity)
  void visit(const Binary&) override {
    auto* bin = static_cast<Binary*>(slot->get());
    if (bin->lhs) { rewrite(bin->lhs); }
    if (bin->rhs) { rewrite(bin->rhs); }
    auto* left = bin->lhs.get();
    auto* right = bin->rhs.get();
    if (isLit<IntLiteral>(left) && isLit<IntLiteral>(right)) {
      const auto a = static_cast<IntLiteral*>(left)->value;
      const auto b = static_cast<IntLiteral*>(right)->value;
      if (foldIntArith(*bin, a, b)) { return; }
      if (foldIntCmp(*bin, a, b)) { return; }
    }
    if (isLit<FloatLiteral>(left) && isLit<FloatLiteral>(right)) {
      const auto a = static_cast<FloatLiteral*>(left)->value;
      const auto b = static_cast<FloatLiteral*>(right)->value;
      if (foldFloatArith(*bin, a, b)) { return; }
      if (foldFloatCmp(*bin, a, b)) { return; }
    }
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
    auto* s = static_cast<IfStmt*>(stmtSlot->get());
    rewrite(s->cond);
    walkBlock(s->thenBody);
    walkBlock(s->elseBody);
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

// NOLINTNEXTLINE(readability-function-size)
size_t ConstantFold::run(Module& module) {
  size_t changes = 0;
  stats_.clear();
  FoldVisitor vis(stats_, changes);
  for (auto& func : module.functions) { vis.walkBlock(func->body); }
  return changes;
}

} // namespace pycc::opt
