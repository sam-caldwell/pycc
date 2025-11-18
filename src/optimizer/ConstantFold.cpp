/***
 * Name: pycc::opt::ConstantFold (impl)
 * Purpose: Fold constant expressions via AST mutation using a visitor.
 */
#include "optimizer/ConstantFold.h"
#include "ast/AssignStmt.h"
#include "ast/Binary.h"
#include "ast/BinaryOperator.h"
#include "ast/BoolLiteral.h"
#include "ast/Call.h"
#include "ast/Expr.h"
#include "ast/ExprStmt.h"
#include "ast/FloatLiteral.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/IntLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/NodeKind.h"
#include "ast/NoneLiteral.h"
#include "ast/ObjectLiteral.h"
#include "ast/ReturnStmt.h"
#include "ast/Stmt.h"
#include "ast/StringLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/Unary.h"
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
using pycc::ast::NodeKind;
using pycc::ast::UnaryOperator;
using pycc::ast::BinaryOperator;

// Simple helpers to classify constant literal nodes
static inline bool isInt(const Expr* expr) { return expr != nullptr && expr->kind == NodeKind::IntLiteral; }
static inline bool isFloat(const Expr* expr) { return expr != nullptr && expr->kind == NodeKind::FloatLiteral; }

namespace {

struct FoldVisitor : public ast::VisitorBase {
  std::unordered_map<std::string, uint64_t>& stats;
  size_t& changes;
  std::unique_ptr<Expr>* slot{nullptr};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};

  FoldVisitor(std::unordered_map<std::string, uint64_t>& statsMap, size_t& changeCount)
      : stats(statsMap), changes(changeCount) {}

  static void assignLoc(Expr& dst, const Expr& src) { dst.file = src.file; dst.line = src.line; dst.col = src.col; }

  // Stats helpers to avoid repeated map lookups/keys
  void bumpUnary() { ++stats["unary"]; }
  void bumpBinInt() { ++stats["binary_int"]; }
  void bumpCmpInt() { ++stats["compare_int"]; }
  void bumpBinFloat() { ++stats["binary_float"]; }
  void bumpCmpFloat() { ++stats["compare_float"]; }

  void rewrite(std::unique_ptr<Expr>& expr) { if (!expr) { return; } slot = &expr; expr->accept(*this); }

  void touch(std::unique_ptr<Stmt>& stmt) { if (!stmt) { return; } stmtSlot = &stmt; stmt->accept(*this); }
  void walkBlock(std::vector<std::unique_ptr<Stmt>>& body) { for (auto& stmt : body) { touch(stmt); } }

  void visit(const Unary& unary) override {
    (void)unary;
    auto* unaryNode = static_cast<Unary*>(slot->get());
    // First rewrite the operand to expose literals; keep a handle to this node's slot
    auto* exprSlot = slot;
    rewrite(unaryNode->operand);
    if (unaryNode->op == UnaryOperator::Neg && unaryNode->operand) {
      if (isInt(unaryNode->operand.get())) {
        auto* lit = static_cast<IntLiteral*>(unaryNode->operand.get());
        auto rep = std::make_unique<IntLiteral>(-lit->value);
        assignLoc(*rep, *unaryNode); slot = exprSlot; *slot = std::move(rep); ++changes; bumpUnary(); return;
      }
      if (isFloat(unaryNode->operand.get())) {
        auto* lit = static_cast<FloatLiteral*>(unaryNode->operand.get());
        auto rep = std::make_unique<FloatLiteral>(-lit->value);
        assignLoc(*rep, *unaryNode); slot = exprSlot; *slot = std::move(rep); ++changes; bumpUnary(); return;
      }
    }
    if (unaryNode->op == UnaryOperator::Not && unaryNode->operand && unaryNode->operand->kind == NodeKind::BoolLiteral) {
      auto* bl = static_cast<BoolLiteral*>(unaryNode->operand.get());
      auto rep = std::make_unique<BoolLiteral>(!bl->value);
      assignLoc(*rep, *unaryNode); slot = exprSlot; *slot = std::move(rep); ++changes; bumpUnary(); return;
    }
  }

  bool foldIntArith(Binary& bin, long long lhs, long long rhs) {
    long long res = 0;
    switch (bin.op) {
      case BinaryOperator::Add: res = lhs + rhs; break;
      case BinaryOperator::Sub: res = lhs - rhs; break;
      case BinaryOperator::Mul: res = lhs * rhs; break;
      case BinaryOperator::Div: if (rhs == 0) { return false; } res = lhs / rhs; break;
      case BinaryOperator::Mod: if (rhs == 0) { return false; } res = lhs % rhs; break;
      case BinaryOperator::FloorDiv: if (rhs == 0) { return false; } res = lhs / rhs; break;
      case BinaryOperator::Pow: {
        if (rhs < 0) { return false; }
        long long acc = 1;
        for (long long i = 0; i < rhs; ++i) { acc *= lhs; }
        res = acc;
        break;
      }
      default: return false;
    }
    bumpBinInt();
    auto rep = std::make_unique<IntLiteral>(res); assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }
  bool foldIntCmp(Binary& bin, long long lhs, long long rhs) {
    std::unique_ptr<Expr> rep;
    switch (bin.op) {
      case BinaryOperator::Eq: rep = std::make_unique<BoolLiteral>(lhs == rhs); break;
      case BinaryOperator::Ne: rep = std::make_unique<BoolLiteral>(lhs != rhs); break;
      case BinaryOperator::Lt: rep = std::make_unique<BoolLiteral>(lhs < rhs); break;
      case BinaryOperator::Le: rep = std::make_unique<BoolLiteral>(lhs <= rhs); break;
      case BinaryOperator::Gt: rep = std::make_unique<BoolLiteral>(lhs > rhs); break;
      case BinaryOperator::Ge: rep = std::make_unique<BoolLiteral>(lhs >= rhs); break;
      default: return false;
    }
    bumpCmpInt();
    assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }
  bool foldFloatArith(Binary& bin, double lhs, double rhs) {
    double res = 0.0;
    switch (bin.op) {
      case BinaryOperator::Add: res = lhs + rhs; break;
      case BinaryOperator::Sub: res = lhs - rhs; break;
      case BinaryOperator::Mul: res = lhs * rhs; break;
      case BinaryOperator::Div: if (rhs == 0.0) { return false; } res = lhs / rhs; break;
      case BinaryOperator::Pow: {
        if (rhs < 0.0) { return false; }
        double acc = 1.0;
        for (int i = 0; i < static_cast<int>(rhs); ++i) { acc *= lhs; }
        res = acc; break;
      }
      default: return false;
    }
    bumpBinFloat();
    auto rep = std::make_unique<FloatLiteral>(res); assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }
  bool foldFloatCmp(Binary& bin, double lhs, double rhs) {
    std::unique_ptr<Expr> rep;
    switch (bin.op) {
      case BinaryOperator::Eq: rep = std::make_unique<BoolLiteral>(lhs == rhs); break;
      case BinaryOperator::Ne: rep = std::make_unique<BoolLiteral>(lhs != rhs); break;
      case BinaryOperator::Lt: rep = std::make_unique<BoolLiteral>(lhs < rhs); break;
      case BinaryOperator::Le: rep = std::make_unique<BoolLiteral>(lhs <= rhs); break;
      case BinaryOperator::Gt: rep = std::make_unique<BoolLiteral>(lhs > rhs); break;
      case BinaryOperator::Ge: rep = std::make_unique<BoolLiteral>(lhs >= rhs); break;
      default: return false;
    }
    bumpCmpFloat();
    assignLoc(*rep, bin); *slot = std::move(rep); ++changes; return true;
  }

  void tryFoldBinary(Binary& bin) {
    auto* left = bin.lhs.get();
    auto* right = bin.rhs.get();
    if (isInt(left) && isInt(right)) {
      const auto lhs = static_cast<IntLiteral*>(left)->value;
      const auto rhs = static_cast<IntLiteral*>(right)->value;
      if (foldIntArith(bin, lhs, rhs)) { return; }
      (void)foldIntCmp(bin, lhs, rhs);
      return;
    }
    if (isFloat(left) && isFloat(right)) {
      const auto lhs = static_cast<FloatLiteral*>(left)->value;
      const auto rhs = static_cast<FloatLiteral*>(right)->value;
      if (foldFloatArith(bin, lhs, rhs)) { return; }
      (void)foldFloatCmp(bin, lhs, rhs);
    }
    // Bitwise and shifts for int literals
    if (left && right && left->kind == NodeKind::IntLiteral && right->kind == NodeKind::IntLiteral) {
      const auto lhs = static_cast<IntLiteral*>(left)->value;
      const auto rhs = static_cast<IntLiteral*>(right)->value;
      long long res = 0; bool ok = true;
      switch (bin.op) {
        case BinaryOperator::BitAnd: res = lhs & rhs; break;
        case BinaryOperator::BitOr: res = lhs | rhs; break;
        case BinaryOperator::BitXor: res = lhs ^ rhs; break;
        case BinaryOperator::LShift: res = lhs << rhs; break;
        case BinaryOperator::RShift: res = lhs >> rhs; break;
        default: ok = false; break;
      }
      if (ok) {
        auto rep = std::make_unique<IntLiteral>(res);
        assignLoc(*rep, bin); *slot = std::move(rep); ++changes; bumpBinInt(); return;
      }
    }
    // Boolean logic when both literals
    if (left && right && left->kind == NodeKind::BoolLiteral && right->kind == NodeKind::BoolLiteral) {
      const bool lb = static_cast<BoolLiteral*>(left)->value;
      const bool rb = static_cast<BoolLiteral*>(right)->value;
      std::unique_ptr<Expr> rep;
      if (bin.op == BinaryOperator::And) rep = std::make_unique<BoolLiteral>(lb && rb);
      else if (bin.op == BinaryOperator::Or) rep = std::make_unique<BoolLiteral>(lb || rb);
      if (rep) { assignLoc(*rep, bin); *slot = std::move(rep); ++changes; bumpUnary(); return; }
    }
  }

  void visit(const Binary& binary) override {
    (void)binary;
    auto* bin = static_cast<Binary*>(slot->get());
    // Save current slot so replacements affect this node, not the last child visited
    auto* exprSlot = slot;
    if (bin->lhs) { rewrite(bin->lhs); }
    if (bin->rhs) { rewrite(bin->rhs); }
    slot = exprSlot;
    tryFoldBinary(*bin);
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
  void visit(const ast::Module& module) override { (void)module; }
  void visit(const ast::FunctionDef& functionDef) override { (void)functionDef; }
  void visit(const ast::ExprStmt& exprStmt) override { (void)exprStmt; }
  void visit(const ast::IntLiteral& intLiteral) override { (void)intLiteral; }
  void visit(const ast::BoolLiteral& boolLiteral) override { (void)boolLiteral; }
  void visit(const ast::FloatLiteral& floatLiteral) override { (void)floatLiteral; }
  void visit(const ast::StringLiteral& stringLiteral) override { (void)stringLiteral; }
  void visit(const ast::NoneLiteral& noneLiteral) override { (void)noneLiteral; }
  void visit(const ast::Name& name) override { (void)name; }
  void visit(const ast::TupleLiteral& tupleLiteral) override { (void)tupleLiteral; }
  void visit(const ast::ListLiteral& listLiteral) override { (void)listLiteral; }
  void visit(const ast::ObjectLiteral& objectLiteral) override { (void)objectLiteral; }
};

} // namespace

size_t ConstantFold::run(Module& module) {
  size_t totalChanges = 0;
  stats_.clear();
  // Iterate to a fixed point so that newly exposed literals are folded
  for (int iter = 0; iter < 4; ++iter) { // small hard cap to avoid infinite loops
    size_t iterationChanges = 0;
    FoldVisitor vis(stats_, iterationChanges);
    for (auto& func : module.functions) { vis.walkBlock(func->body); }
    totalChanges += iterationChanges;
    if (iterationChanges == 0) { break; }
  }
  return totalChanges;
}

} // namespace pycc::opt
