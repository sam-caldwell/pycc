/***
 * Name: pycc::opt::LocalProp (impl)
 */
#include "optimizer/LocalProp.h"

#include "ast/AssignStmt.h"
#include "ast/AugAssignStmt.h"
#include "ast/Binary.h"
#include "ast/BoolLiteral.h"
#include "ast/Call.h"
#include "ast/ClassDef.h"
#include "ast/DictLiteral.h"
#include "ast/Expr.h"
#include "ast/ExprStmt.h"
#include "ast/FloatLiteral.h"
#include "ast/ForStmt.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/IntLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/MatchStmt.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/NonlocalStmt.h"
#include "ast/ObjectLiteral.h"
#include "ast/ReturnStmt.h"
#include "ast/SetLiteral.h"
#include "ast/StringLiteral.h"
#include "ast/Subscript.h"
#include "ast/TryStmt.h"
#include "ast/TupleLiteral.h"
#include "ast/Unary.h"
#include "ast/VisitorBase.h"
#include "ast/WhileStmt.h"
#include "ast/WithStmt.h"

#include <cstddef>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pycc::opt {
using namespace pycc::ast;

namespace {

struct ConstVal {
  NodeKind kind{NodeKind::NoneLiteral};
  long long i{};
  double f{};
  bool b{};
  std::string s{};
};

struct Env {
  // Copy-prop map: name -> other name
  std::unordered_map<std::string, std::string> alias;
  // Constant map: name -> literal value
  std::unordered_map<std::string, ConstVal> konst;

  void kill(const std::string& name) {
    alias.erase(name);
    konst.erase(name);
  }
  void clear() {
    alias.clear();
    konst.clear();
  }
  // Follow alias chain to a base name
  std::string rootOf(const std::string& name) const {
    std::string cur = name;
    int guard = 0;
    while (true) {
      auto it = alias.find(cur);
      if (it == alias.end()) break;
      if (++guard > 64) break; // cycle guard
      cur = it->second;
    }
    return cur;
  }
  std::optional<ConstVal> constOf(const std::string& name) const {
    const std::string base = rootOf(name);
    auto it = konst.find(base);
    if (it == konst.end()) return std::nullopt; return it->second;
  }
};

struct Rewriter : public ast::VisitorBase {
  Env& env;
  std::size_t& changes;
  std::unique_ptr<Expr>* exprSlot{nullptr};
  std::unique_ptr<Stmt>* stmtSlot{nullptr};
  explicit Rewriter(Env& e, std::size_t& c) : env(e), changes(c) {}

  // Helpers
  static std::unique_ptr<Expr> makeLiteral(const ConstVal& v) {
    switch (v.kind) {
      case NodeKind::IntLiteral: return std::make_unique<IntLiteral>(v.i);
      case NodeKind::FloatLiteral: return std::make_unique<FloatLiteral>(v.f);
      case NodeKind::BoolLiteral: return std::make_unique<BoolLiteral>(v.b);
      case NodeKind::StringLiteral: return std::make_unique<StringLiteral>(v.s);
      default: break;
    }
    return nullptr;
  }
  void rewriteExpr(std::unique_ptr<Expr>& e) { if (!e) return; exprSlot = &e; e->accept(*this); }
  void touchStmt(std::unique_ptr<Stmt>& s) { if (!s) return; stmtSlot = &s; s->accept(*this); }

  // Expressions
  void visit(const Name& n) override {
    auto* nameNode = static_cast<Name*>(exprSlot->get());
    // Try constant first
    if (auto cv = env.constOf(n.id)) {
      auto rep = makeLiteral(*cv);
      if (rep) { rep->line = nameNode->line; rep->col = nameNode->col; rep->file = nameNode->file; *exprSlot = std::move(rep); ++changes; return; }
    }
    // Then copy-prop root name
    const std::string root = env.rootOf(n.id);
    if (root != n.id) {
      auto rep = std::make_unique<Name>(root);
      rep->line = nameNode->line; rep->col = nameNode->col; rep->file = nameNode->file;
      *exprSlot = std::move(rep); ++changes; return;
    }
  }
  void visit(const Unary& /*u*/) override {
    auto* un = static_cast<Unary*>(exprSlot->get());
    rewriteExpr(un->operand);
  }
  void visit(const Binary& /*b*/) override {
    auto* bin = static_cast<Binary*>(exprSlot->get());
    rewriteExpr(bin->lhs);
    rewriteExpr(bin->rhs);
  }
  void visit(const Call& /*c*/) override {
    auto* call = static_cast<Call*>(exprSlot->get());
    rewriteExpr(call->callee);
    for (auto& a : call->args) rewriteExpr(a);
    for (auto& s : call->starArgs) rewriteExpr(s);
    for (auto& s : call->kwStarArgs) rewriteExpr(s);
    for (auto& k : call->keywords) { if (k.value) rewriteExpr(k.value); }
  }
  void visit(const TupleLiteral& /*t*/) override {
    auto* tp = static_cast<TupleLiteral*>(exprSlot->get());
    for (auto& el : tp->elements) rewriteExpr(el);
  }
  void visit(const ListLiteral& /*l*/) override {
    auto* lst = static_cast<ListLiteral*>(exprSlot->get());
    for (auto& el : lst->elements) rewriteExpr(el);
  }
  void visit(const SetLiteral& /*s*/) override {
    auto* st = static_cast<SetLiteral*>(exprSlot->get());
    for (auto& el : st->elements) rewriteExpr(el);
  }
  void visit(const DictLiteral& /*d*/) override {
    auto* dl = static_cast<DictLiteral*>(exprSlot->get());
    for (auto& kv : dl->items) { rewriteExpr(kv.first); rewriteExpr(kv.second); }
    for (auto& u : dl->unpacks) rewriteExpr(u);
  }
  void visit(const ObjectLiteral& /*o*/) override {
    auto* obj = static_cast<ObjectLiteral*>(exprSlot->get());
    for (auto& f : obj->fields) rewriteExpr(f);
  }
  void visit(const Subscript& /*s*/) override {
    auto* sub = static_cast<Subscript*>(exprSlot->get());
    rewriteExpr(sub->value);
    rewriteExpr(sub->slice);
  }
  void visit(const FStringLiteral&) override {}
  void visit(const NoneLiteral&) override {}
  void visit(const IntLiteral&) override {}
  void visit(const FloatLiteral&) override {}
  void visit(const BoolLiteral&) override {}
  void visit(const StringLiteral&) override {}
  void visit(const AwaitExpr&) override {}
  void visit(const YieldExpr&) override {}
  void visit(const Compare& /*c*/) override {
    auto* cmp = static_cast<Compare*>(exprSlot->get());
    rewriteExpr(cmp->left);
    for (auto& e : cmp->comparators) rewriteExpr(e);
  }

  // Statements
  void visit(const ExprStmt& /*es*/) override {
    auto* st = static_cast<ExprStmt*>(stmtSlot->get());
    rewriteExpr(st->value);
  }
  void visit(const ReturnStmt& /*rs*/) override {
    auto* st = static_cast<ReturnStmt*>(stmtSlot->get());
    rewriteExpr(st->value);
  }
  void visit(const AssignStmt& /*as*/) override {
    auto* st = static_cast<AssignStmt*>(stmtSlot->get());
    rewriteExpr(st->value);
    // Kill by default; re-define simple name targets after
    auto killTarget = [&](const Expr* tgt) {
      if (!tgt) return;
      if (tgt->kind == NodeKind::Name) {
        const auto* nm = static_cast<const Name*>(tgt);
        env.kill(nm->id);
      }
    };
    if (!st->targets.empty()) {
      for (const auto& t : st->targets) killTarget(t.get());
    } else if (!st->target.empty()) { env.kill(st->target); }

    // Simple propagation when RHS is a literal or name and LHS is a simple name
    auto defineConst = [&](const std::string& lhs, const ConstVal& v) { env.alias.erase(lhs); env.konst[lhs] = v; };
    auto defineAlias = [&](const std::string& lhs, const std::string& rhs) { env.konst.erase(lhs); env.alias[lhs] = env.rootOf(rhs); };

    if (!st->targets.empty() && st->targets.size() == 1 && st->targets[0] && st->targets[0]->kind == NodeKind::Name) {
      const auto* nm = static_cast<const Name*>(st->targets[0].get());
      if (st->value) {
        switch (st->value->kind) {
          case NodeKind::IntLiteral: defineConst(nm->id, ConstVal{NodeKind::IntLiteral, static_cast<const IntLiteral*>(st->value.get())->value, 0.0, false, {}}); break;
          case NodeKind::FloatLiteral: defineConst(nm->id, ConstVal{NodeKind::FloatLiteral, 0, static_cast<const FloatLiteral*>(st->value.get())->value, false, {}}); break;
          case NodeKind::BoolLiteral: defineConst(nm->id, ConstVal{NodeKind::BoolLiteral, 0, 0.0, static_cast<const BoolLiteral*>(st->value.get())->value, {}}); break;
          case NodeKind::StringLiteral: defineConst(nm->id, ConstVal{NodeKind::StringLiteral, 0, 0.0, false, static_cast<const StringLiteral*>(st->value.get())->value}); break;
          case NodeKind::Name: defineAlias(nm->id, static_cast<const Name*>(st->value.get())->id); break;
          default: break;
        }
      }
    } else if (st->targets.empty() && !st->target.empty()) {
      const std::string& lhs = st->target;
      if (st->value) {
        switch (st->value->kind) {
          case NodeKind::IntLiteral: defineConst(lhs, ConstVal{NodeKind::IntLiteral, static_cast<const IntLiteral*>(st->value.get())->value, 0.0, false, {}}); break;
          case NodeKind::FloatLiteral: defineConst(lhs, ConstVal{NodeKind::FloatLiteral, 0, static_cast<const FloatLiteral*>(st->value.get())->value, false, {}}); break;
          case NodeKind::BoolLiteral: defineConst(lhs, ConstVal{NodeKind::BoolLiteral, 0, 0.0, static_cast<const BoolLiteral*>(st->value.get())->value, {}}); break;
          case NodeKind::StringLiteral: defineConst(lhs, ConstVal{NodeKind::StringLiteral, 0, 0.0, false, static_cast<const StringLiteral*>(st->value.get())->value}); break;
          case NodeKind::Name: defineAlias(lhs, static_cast<const Name*>(st->value.get())->id); break;
          default: break;
        }
      }
    } else {
      // Complex targets: reset env conservatively
      env.clear();
    }
  }
  void visit(const AugAssignStmt& /*ag*/) override {
    auto* st = static_cast<AugAssignStmt*>(stmtSlot->get());
    rewriteExpr(st->value);
    // Kill target name
    if (st->target && st->target->kind == NodeKind::Name) {
      const auto* nm = static_cast<const Name*>(st->target.get()); env.kill(nm->id);
    } else { env.clear(); }
  }

  void visit(const IfStmt& /*is*/) override {
    auto* st = static_cast<IfStmt*>(stmtSlot->get());
    rewriteExpr(st->cond);
    // Recurse with fresh env; do not merge back
    Env save = env; Env thenEnv; Env elseEnv;
    env = thenEnv; for (auto& s : st->thenBody) touchStmt(s);
    env = elseEnv; for (auto& s : st->elseBody) touchStmt(s);
    env = save; env.clear();
  }
  void visit(const WhileStmt& /*ws*/) override {
    auto* st = static_cast<WhileStmt*>(stmtSlot->get());
    rewriteExpr(st->cond);
    Env save = env; env.clear();
    for (auto& s : st->thenBody) touchStmt(s);
    for (auto& s : st->elseBody) touchStmt(s);
    env = save; env.clear();
  }
  void visit(const ForStmt& /*fs*/) override {
    auto* st = static_cast<ForStmt*>(stmtSlot->get());
    rewriteExpr(st->target);
    rewriteExpr(st->iterable);
    Env save = env; env.clear();
    for (auto& s : st->thenBody) touchStmt(s);
    for (auto& s : st->elseBody) touchStmt(s);
    env = save; env.clear();
  }
  void visit(const TryStmt& /*ts*/) override {
    auto* st = static_cast<TryStmt*>(stmtSlot->get());
    Env save = env; env.clear();
    for (auto& s : st->body) touchStmt(s);
    for (auto& h : st->handlers) { if (h) { for (auto& s : h->body) touchStmt(s); } }
    for (auto& s : st->orelse) touchStmt(s);
    for (auto& s : st->finalbody) touchStmt(s);
    env = save; env.clear();
  }
  void visit(const WithStmt& /*ws*/) override {
    auto* st = static_cast<WithStmt*>(stmtSlot->get());
    for (auto& it : st->items) { if (it) rewriteExpr(it->context); }
    Env save = env; env.clear();
    for (auto& s : st->body) touchStmt(s);
    env = save; env.clear();
  }
  void visit(const MatchStmt& /*ms*/) override {
    auto* st = static_cast<MatchStmt*>(stmtSlot->get());
    rewriteExpr(st->subject);
    Env save = env; env.clear();
    for (auto& c : st->cases) { if (c) { if (c->guard) rewriteExpr(c->guard); for (auto& s : c->body) touchStmt(s); } }
    env = save; env.clear();
  }
  void visit(const FunctionDef& fn) override { (void)fn; }
  void visit(const ClassDef& cd) override { (void)cd; env.clear(); }
  void visit(const Module& m) override { (void)m; }
  void visit(const GlobalStmt& g) override { (void)g; env.clear(); }
  void visit(const NonlocalStmt& nl) override { (void)nl; env.clear(); }
  void visit(const Import&) override { env.clear(); }
  void visit(const ImportFrom&) override { env.clear(); }
  void visit(const DefStmt&) override { env.clear(); }
  void visit(const RaiseStmt&) override { env.clear(); }
  void visit(const AssertStmt&) override {}
  void visit(const DelStmt&) override { env.clear(); }
  void visit(const BreakStmt&) override {}
  void visit(const ContinueStmt&) override {}
  void visit(const PassStmt&) override {}
};

static std::size_t runOnBlock(std::vector<std::unique_ptr<Stmt>>& body) {
  Env env;
  std::size_t changes = 0;
  Rewriter rw{env, changes};
  for (auto& s : body) { rw.touchStmt(s); }
  return changes;
}

} // namespace

std::size_t LocalProp::run(Module& module) {
  std::size_t changes = 0;
  for (auto& fn : module.functions) { changes += runOnBlock(fn->body); }
  // Classes: do not cross function boundaries, so ignore class locals here
  return changes;
}

} // namespace pycc::opt
