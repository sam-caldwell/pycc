/***
 * Name: pycc::opt::ConstantFold (impl)
 * Purpose: Fold constant expressions via AST mutation using a visitor.
 */
#include "optimizer/ConstantFold.h"
#include "ast/VisitorBase.h"
#include <memory>

namespace pycc::opt {

using pycc::ast::Module;
using pycc::ast::Stmt;
using pycc::ast::Expr;
using pycc::ast::Binary;
using pycc::ast::Unary;
using pycc::ast::Call;
using pycc::ast::AssignStmt;
using pycc::ast::ReturnStmt;
using pycc::ast::IfStmt;
using pycc::ast::IntLiteral;
using pycc::ast::FloatLiteral;
using pycc::ast::BoolLiteral;
using pycc::ast::Name;
using pycc::ast::TupleLiteral;
using pycc::ast::ListLiteral;
using pycc::ast::ObjectLiteral;
using pycc::ast::ExprStmt;
using pycc::ast::FunctionDef;
using pycc::ast::NodeKind;
using pycc::ast::UnaryOperator;
using pycc::ast::BinaryOperator;

namespace {
inline bool isInt(const Expr* e) { return e != nullptr && e->kind == NodeKind::IntLiteral; }
inline bool isFloat(const Expr* e) { return e != nullptr && e->kind == NodeKind::FloatLiteral; }

struct FoldVisitor : public ast::VisitorBase {
  std::unordered_map<std::string, uint64_t>& stats;
  size_t& changes;
  std::unique_ptr<Expr>* slot{nullptr};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};

  FoldVisitor(std::unordered_map<std::string, uint64_t>& s, size_t& c) : stats(s), changes(c) {}

  static void assignLoc(Expr& dst, const Expr& src) { dst.file = src.file; dst.line = src.line; dst.col = src.col; }

  // Stats helpers to avoid repeated map lookups/keys
  void bumpUnary() { ++stats["unary"]; }
  void bumpBinInt() { ++stats["binary_int"]; }
  void bumpCmpInt() { ++stats["compare_int"]; }
  void bumpBinFloat() { ++stats["binary_float"]; }
  void bumpCmpFloat() { ++stats["compare_float"]; }

  void rewrite(std::unique_ptr<Expr>& e) { if (!e) { return; } slot = &e; e->accept(*this); }

  void touch(std::unique_ptr<Stmt>& s) { if (!s) { return; } stmtSlot = &s; s->accept(*this); }
  void walkBlock(std::vector<std::unique_ptr<Stmt>>& body) { for (auto& st : body) { touch(st); } }

  void visit(const Unary&) override {
    auto* un = static_cast<Unary*>(slot->get());
    if (un->op == UnaryOperator::Neg && un->operand) {
      if (isInt(un->operand.get())) {
        auto* lit = static_cast<IntLiteral*>(un->operand.get());
        auto rep = std::make_unique<IntLiteral>(-lit->value);
        assignLoc(*rep, *un); *slot = std::move(rep); ++changes; bumpUnary(); return;
      }
      if (isFloat(un->operand.get())) {
        auto* lit = static_cast<FloatLiteral*>(un->operand.get());
        auto rep = std::make_unique<FloatLiteral>(-lit->value);
        assignLoc(*rep, *un); *slot = std::move(rep); ++changes; bumpUnary(); return;
      }
    }
    rewrite(un->operand);
  }

  bool foldIntArith(Binary& bin, long long a, long long b) {
    long long res = 0;
    switch (bin.op) {
      case BinaryOperator::Add: res = a + b; break;
      case BinaryOperator::Sub: res = a - b; break;
      case BinaryOperator::Mul: res = a * b; break;
      case BinaryOperator::Div: if (b == 0) { return false; } res = a / b; break;
      case BinaryOperator::Mod: if (b == 0) { return false; } res = a % b; break;
      default: return false;
    }
    bumpBinInt();
    auto rep = std::make_unique<IntLiteral>(res); assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }
  bool foldIntCmp(Binary& bin, long long a, long long b) {
    std::unique_ptr<Expr> rep;
    switch (bin.op) {
      case BinaryOperator::Eq: rep = std::make_unique<BoolLiteral>(a == b); break;
      case BinaryOperator::Ne: rep = std::make_unique<BoolLiteral>(a != b); break;
      case BinaryOperator::Lt: rep = std::make_unique<BoolLiteral>(a < b); break;
      case BinaryOperator::Le: rep = std::make_unique<BoolLiteral>(a <= b); break;
      case BinaryOperator::Gt: rep = std::make_unique<BoolLiteral>(a > b); break;
      case BinaryOperator::Ge: rep = std::make_unique<BoolLiteral>(a >= b); break;
      default: return false;
    }
    bumpCmpInt();
    assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }
  bool foldFloatArith(Binary& bin, double a, double b) {
    double res = 0.0;
    switch (bin.op) {
      case BinaryOperator::Add: res = a + b; break;
      case BinaryOperator::Sub: res = a - b; break;
      case BinaryOperator::Mul: res = a * b; break;
      case BinaryOperator::Div: if (b == 0.0) { return false; } res = a / b; break;
      default: return false;
    }
    bumpBinFloat();
    auto rep = std::make_unique<FloatLiteral>(res); assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }
  bool foldFloatCmp(Binary& bin, double a, double b) {
    std::unique_ptr<Expr> rep;
    switch (bin.op) {
      case BinaryOperator::Eq: rep = std::make_unique<BoolLiteral>(a == b); break;
      case BinaryOperator::Ne: rep = std::make_unique<BoolLiteral>(a != b); break;
      case BinaryOperator::Lt: rep = std::make_unique<BoolLiteral>(a < b); break;
      case BinaryOperator::Le: rep = std::make_unique<BoolLiteral>(a <= b); break;
      case BinaryOperator::Gt: rep = std::make_unique<BoolLiteral>(a > b); break;
      case BinaryOperator::Ge: rep = std::make_unique<BoolLiteral>(a >= b); break;
      default: return false;
    }
    bumpCmpFloat();
    assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }

  void tryFoldBinary(Binary& bin) {
    auto* left = bin.lhs.get();
    auto* right = bin.rhs.get();
    if (isInt(left) && isInt(right)) {
      const auto a = static_cast<IntLiteral*>(left)->value;
      const auto b = static_cast<IntLiteral*>(right)->value;
      if (foldIntArith(bin, a, b)) { return; }
      (void)foldIntCmp(bin, a, b);
      return;
    }
    if (isFloat(left) && isFloat(right)) {
      const auto a = static_cast<FloatLiteral*>(left)->value;
      const auto b = static_cast<FloatLiteral*>(right)->value;
      if (foldFloatArith(bin, a, b)) { return; }
      (void)foldFloatCmp(bin, a, b);
    }
  }

  void visit(const Binary&) override {
    auto* bin = static_cast<Binary*>(slot->get());
    if (bin->lhs) { rewrite(bin->lhs); }
    if (bin->rhs) { rewrite(bin->rhs); }
    tryFoldBinary(*bin);
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
  void visit(const ast::Module&) override {}
  void visit(const ast::FunctionDef&) override {}
  void visit(const ast::ExprStmt&) override {}
  void visit(const ast::IntLiteral&) override {}
  void visit(const ast::BoolLiteral&) override {}
  void visit(const ast::FloatLiteral&) override {}
  void visit(const ast::StringLiteral&) override {}
  void visit(const ast::NoneLiteral&) override {}
  void visit(const ast::Name&) override {}
  void visit(const ast::TupleLiteral&) override {}
  void visit(const ast::ListLiteral&) override {}
  void visit(const ast::ObjectLiteral&) override {}
};

} // namespace

size_t ConstantFold::run(Module& module) {
  size_t changes = 0;
  stats_.clear();
  FoldVisitor vis(stats_, changes);
  for (auto& func : module.functions) { vis.walkBlock(func->body); }
  return changes;
}

} // namespace pycc::opt
