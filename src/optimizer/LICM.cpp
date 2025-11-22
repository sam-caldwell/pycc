/***
 * Name: pycc::opt::LICM (impl)
 */
#include "optimizer/LICM.h"

#include "optimizer/EffectAlias.h"
#include "ast/Module.h"
#include "ast/FunctionDef.h"
#include "ast/WhileStmt.h"
#include "ast/AssignStmt.h"
#include "ast/Name.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace pycc::opt {
using namespace pycc::ast;

namespace {
static bool isInvariantRHS(const Expr* e) {
  return EffectAlias::isPureExpr(e);
}

static void collectAssignedNames(const std::vector<std::unique_ptr<Stmt>>& stmts, std::unordered_map<std::string,int>& writes) {
  for (const auto& s : stmts) {
    if (!s) continue;
    if (s->kind == NodeKind::AssignStmt) {
      const auto* as = static_cast<const AssignStmt*>(s.get());
      if (!as->targets.empty()) {
        for (const auto& t : as->targets) if (t && t->kind == NodeKind::Name) writes[static_cast<const Name*>(t.get())->id]++;
      } else if (!as->target.empty()) { writes[as->target]++; }
    }
  }
}

static void collectReadNames(const Expr* e, std::unordered_set<std::string>& reads) {
  if (!e) return;
  switch (e->kind) {
    case NodeKind::Name: reads.insert(static_cast<const Name*>(e)->id); break;
    case NodeKind::UnaryExpr: collectReadNames(static_cast<const Unary*>(e)->operand.get(), reads); break;
    case NodeKind::BinaryExpr: collectReadNames(static_cast<const Binary*>(e)->lhs.get(), reads); collectReadNames(static_cast<const Binary*>(e)->rhs.get(), reads); break;
    case NodeKind::TupleLiteral: { auto* t = static_cast<const TupleLiteral*>(e); for (const auto& el : t->elements) collectReadNames(el.get(), reads); break; }
    case NodeKind::ListLiteral: { auto* l = static_cast<const ListLiteral*>(e); for (const auto& el : l->elements) collectReadNames(el.get(), reads); break; }
    default: break;
  }
}

static std::size_t runOnBlock(std::vector<std::unique_ptr<Stmt>>& body) {
  std::size_t hoisted = 0;
  for (auto it = body.begin(); it != body.end(); ++it) {
    if ((*it)->kind != NodeKind::WhileStmt) continue;
    auto* ws = static_cast<WhileStmt*>(it->get());
    // Gather alias/effect guards
    std::unordered_map<std::string,int> writesInLoop; collectAssignedNames(ws->thenBody, writesInLoop);
    // Scan loop body for invariant assignments of form name = <pure expr> anywhere in body
    for (auto bit = ws->thenBody.begin(); bit != ws->thenBody.end();) {
      Stmt* s = bit->get();
      if (s->kind != NodeKind::AssignStmt) { ++bit; continue; }
      auto* asg = static_cast<AssignStmt*>(s);
      // Only simple name targets
      std::string targetName;
      if (!asg->targets.empty() && asg->targets.size() == 1 && asg->targets[0] && asg->targets[0]->kind == NodeKind::Name) {
        targetName = static_cast<const Name*>(asg->targets[0].get())->id;
      } else if (asg->targets.empty() && !asg->target.empty()) {
        targetName = asg->target;
      } else { ++bit; continue; }
      if (!asg->value || !isInvariantRHS(asg->value.get())) { ++bit; continue; }
      // Don't hoist if target assigned multiple times in loop
      if (writesInLoop[targetName] > 1) { ++bit; continue; }
      // Don't hoist if RHS reads any name assigned within loop
      std::unordered_set<std::string> reads; collectReadNames(asg->value.get(), reads);
      bool dependsOnLoopWrite = false; for (const auto& r : reads) { if (writesInLoop.count(r)) { dependsOnLoopWrite = true; break; } }
      if (dependsOnLoopWrite) { ++bit; continue; }
      // Ensure target name is not read before this assignment within loop body (simple scan)
      bool usedBefore = false;
      for (auto bit2 = ws->thenBody.begin(); bit2 != bit; ++bit2) {
        // scan for name read in expressions
        struct Reader : public ast::VisitorBase {
          const std::string& needle; bool found{false};
          explicit Reader(const std::string& n) : needle(n) {}
          // Required pure virtual stubs
          void visit(const Module&) override {}
          void visit(const FunctionDef&) override {}
          void visit(const ReturnStmt&) override {}
          void visit(const AssignStmt& as) override { if (as.value) as.value->accept(*this); }
          void visit(const IfStmt&) override {}
          void visit(const ExprStmt& es) override { if (es.value) es.value->accept(*this); }
          void visit(const IntLiteral&) override {}
          void visit(const FloatLiteral&) override {}
          void visit(const BoolLiteral&) override {}
          void visit(const StringLiteral&) override {}
          void visit(const NoneLiteral&) override {}
          void visit(const Name& n) override { if (n.id == needle) found = true; }
          void visit(const Call&) override {}
          void visit(const Binary& b) override { if (b.lhs) b.lhs->accept(*this); if (b.rhs) b.rhs->accept(*this); }
          void visit(const Unary& u) override { if (u.operand) u.operand->accept(*this); }
          void visit(const TupleLiteral& t) override { for (const auto& el : t.elements) if (el) el->accept(*this); }
          void visit(const ListLiteral& l) override { for (const auto& el : l.elements) if (el) el->accept(*this); }
          void visit(const ObjectLiteral&) override {}
        } rd{targetName};
        (*bit2)->accept(rd); if (rd.found) { usedBefore = true; break; }
      }
      if (usedBefore) { ++bit; continue; }
      // Hoist this assignment before the loop
      auto moved = std::move(*bit);
      bit = ws->thenBody.erase(bit);
      // Insert before the current while-statement. Inserting into 'body' invalidates iterators,
      // so recompute 'it' after insertion using indices.
      const std::size_t widx = static_cast<std::size_t>(std::distance(body.begin(), it));
      body.insert(body.begin() + static_cast<long>(widx), std::move(moved));
      ++hoisted;
      // Reacquire iterator to the while-statement (now shifted by +1)
      it = body.begin() + static_cast<long>(widx + 1);
      ws = static_cast<WhileStmt*>(it->get());
      // Restart scan conservatively from loop start
      bit = ws->thenBody.begin();
    }
  }
  return hoisted;
}
} // namespace

std::size_t LICM::run(Module& module) {
  std::size_t hoisted = 0;
  for (auto& fn : module.functions) { hoisted += runOnBlock(fn->body); }
  return hoisted;
}

} // namespace pycc::opt
