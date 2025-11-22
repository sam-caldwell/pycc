/***
 * Name: computeReturnParamIdxs
 * Purpose: Determine functions that consistently return one of their parameters.
 */
#include "sema/detail/ReturnParamScan.h"
#include "ast/VisitorBase.h"
#include "ast/FunctionDef.h"
#include "ast/ReturnStmt.h"
#include "ast/IfStmt.h"
#include "ast/Name.h"

using namespace pycc;
using namespace pycc::sema;

namespace {
struct RetIdxVisitor : public ast::VisitorBase {
  const ast::FunctionDef* fn{nullptr};
  int retIdx{-1}; bool hasReturn{false}; bool consistent{true};
  void visit(const ast::ReturnStmt& ret) override {
    if (!consistent) { return; }
    hasReturn = true;
    if (!(ret.value && ret.value->kind == ast::NodeKind::Name)) { consistent = false; return; }
    const auto* nameNode = static_cast<const ast::Name*>(ret.value.get());
    int idxFound = -1;
    for (size_t i = 0; i < fn->params.size(); ++i) { if (fn->params[i].name == nameNode->id) { idxFound = static_cast<int>(i); break; } }
    if (idxFound < 0) { consistent = false; return; }
    if (retIdx < 0) { retIdx = idxFound; }
    else if (retIdx != idxFound) { consistent = false; }
  }
  void visit(const ast::IfStmt& iff) override {
    for (const auto& stmt : iff.thenBody) { stmt->accept(*this); }
    for (const auto& stmt : iff.elseBody) { stmt->accept(*this); }
  }
  // Stubs
  void visit(const ast::Module&) override {}
  void visit(const ast::FunctionDef&) override {}
  void visit(const ast::AssignStmt&) override {}
  void visit(const ast::ExprStmt&) override {}
  void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>&) override {}
  void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral>&) override {}
  void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>&) override {}
  void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>&) override {}
  void visit(const ast::NoneLiteral&) override {}
  void visit(const ast::Name&) override {}
  void visit(const ast::Call&) override {}
  void visit(const ast::Binary&) override {}
  void visit(const ast::Unary&) override {}
  void visit(const ast::TupleLiteral&) override {}
  void visit(const ast::ListLiteral&) override {}
  void visit(const ast::ObjectLiteral&) override {}
};
} // namespace

std::unordered_map<std::string, int> computeReturnParamIdxs(const ast::Module& mod) {
  std::unordered_map<std::string, int> retParamIdxs;
  for (const auto& func : mod.functions) {
    RetIdxVisitor visitor; visitor.fn = func.get();
    for (const auto& stmt : func->body) { stmt->accept(visitor); if (!visitor.consistent) { break; } }
    if (visitor.hasReturn && visitor.consistent && visitor.retIdx >= 0) { retParamIdxs[func->name] = visitor.retIdx; }
  }
  return retParamIdxs;
}

