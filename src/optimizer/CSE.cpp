/***
 * Name: pycc::opt::CSE (impl)
 */
#include "optimizer/CSE.h"

#include "optimizer/EffectAlias.h"
#include "ast/Module.h"
#include "ast/FunctionDef.h"
#include "ast/ExprStmt.h"
#include "ast/AssignStmt.h"
#include "ast/Binary.h"
#include "ast/Unary.h"
#include "ast/TupleLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/IntLiteral.h"
#include "ast/FloatLiteral.h"
#include "ast/BoolLiteral.h"
#include "ast/StringLiteral.h"
#include "ast/Name.h"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <unordered_set>

namespace pycc::opt {
using namespace pycc::ast;

namespace {
static void hashExpr(const Expr* e, std::string& out) {
  if (!e) { out += "<null>"; return; }
  out.push_back('#'); out += std::to_string(static_cast<int>(e->kind)); out.push_back(':');
  switch (e->kind) {
    case NodeKind::IntLiteral: out += std::to_string(static_cast<const IntLiteral*>(e)->value); break;
    case NodeKind::FloatLiteral: out += std::to_string(static_cast<const FloatLiteral*>(e)->value); break;
    case NodeKind::BoolLiteral: out += (static_cast<const BoolLiteral*>(e)->value ? "1" : "0"); break;
    case NodeKind::StringLiteral: out += static_cast<const StringLiteral*>(e)->value; break;
    case NodeKind::UnaryExpr: hashExpr(static_cast<const Unary*>(e)->operand.get(), out); break;
    case NodeKind::BinaryExpr: {
      const auto* b = static_cast<const Binary*>(e);
      out += std::to_string(static_cast<int>(b->op));
      hashExpr(b->lhs.get(), out); hashExpr(b->rhs.get(), out);
      break;
    }
    case NodeKind::TupleLiteral: {
      const auto* t = static_cast<const TupleLiteral*>(e);
      for (const auto& el : t->elements) hashExpr(el.get(), out);
      break;
    }
    default: out += "?"; break;
  }
}

static std::size_t runOnBlock(std::vector<std::unique_ptr<Stmt>>& body) {
  std::unordered_set<std::string> seen;
  std::size_t removed = 0;
  for (auto it = body.begin(); it != body.end();) {
    Stmt* s = it->get();
    bool erase = false;
    if (s && s->kind == NodeKind::ExprStmt) {
      auto* es = static_cast<ExprStmt*>(s);
      if (EffectAlias::isPureExpr(es->value.get())) {
        std::string key; key.reserve(64); hashExpr(es->value.get(), key);
        if (!key.empty()) {
          if (seen.find(key) != seen.end()) { erase = true; }
          else { seen.insert(key); }
        }
      }
    }
    if (erase) { it = body.erase(it); ++removed; }
    else { ++it; }
  }
  return removed;
}

static int exprComplexity(const Expr* e) {
  if (!e) return 0;
  switch (e->kind) {
    case NodeKind::IntLiteral:
    case NodeKind::FloatLiteral:
    case NodeKind::BoolLiteral:
    case NodeKind::StringLiteral:
    case NodeKind::Name:
      return 1;
    case NodeKind::UnaryExpr:
      return 1 + exprComplexity(static_cast<const Unary*>(e)->operand.get());
    case NodeKind::BinaryExpr: {
      auto* b = static_cast<const Binary*>(e);
      return 1 + exprComplexity(b->lhs.get()) + exprComplexity(b->rhs.get());
    }
    case NodeKind::TupleLiteral: {
      int acc = 1; auto* t = static_cast<const TupleLiteral*>(e); for (auto& el : t->elements) acc += exprComplexity(el.get()); return acc;
    }
    case NodeKind::ListLiteral: {
      int acc = 1; auto* l = static_cast<const ListLiteral*>(e); for (auto& el : l->elements) acc += exprComplexity(el.get()); return acc;
    }
    default: return 1;
  }
}

static std::unique_ptr<Expr> cloneExpr(const Expr* e) {
  if (!e) return nullptr;
  switch (e->kind) {
    case NodeKind::IntLiteral: return std::make_unique<IntLiteral>(static_cast<const IntLiteral*>(e)->value);
    case NodeKind::FloatLiteral: return std::make_unique<FloatLiteral>(static_cast<const FloatLiteral*>(e)->value);
    case NodeKind::BoolLiteral: return std::make_unique<BoolLiteral>(static_cast<const BoolLiteral*>(e)->value);
    case NodeKind::StringLiteral: return std::make_unique<StringLiteral>(static_cast<const StringLiteral*>(e)->value);
    case NodeKind::Name: return std::make_unique<Name>(static_cast<const Name*>(e)->id);
    case NodeKind::UnaryExpr: {
      auto* u = static_cast<const Unary*>(e);
      auto c = std::make_unique<Unary>(u->op, cloneExpr(u->operand.get())); return c;
    }
    case NodeKind::BinaryExpr: {
      auto* b = static_cast<const Binary*>(e);
      return std::make_unique<Binary>(b->op, cloneExpr(b->lhs.get()), cloneExpr(b->rhs.get()));
    }
    case NodeKind::TupleLiteral: {
      auto* t = static_cast<const TupleLiteral*>(e);
      auto c = std::make_unique<TupleLiteral>();
      for (const auto& el : t->elements) c->elements.emplace_back(cloneExpr(el.get()));
      return c;
    }
    case NodeKind::ListLiteral: {
      auto* l = static_cast<const ListLiteral*>(e);
      auto c = std::make_unique<ListLiteral>();
      for (const auto& el : l->elements) c->elements.emplace_back(cloneExpr(el.get()));
      return c;
    }
    default: return nullptr;
  }
}

static bool equalExpr(const Expr* a, const Expr* b) {
  if (a == b) return true; if (!a || !b) return false; if (a->kind != b->kind) return false;
  switch (a->kind) {
    case NodeKind::IntLiteral: return static_cast<const IntLiteral*>(a)->value == static_cast<const IntLiteral*>(b)->value;
    case NodeKind::FloatLiteral: return static_cast<const FloatLiteral*>(a)->value == static_cast<const FloatLiteral*>(b)->value;
    case NodeKind::BoolLiteral: return static_cast<const BoolLiteral*>(a)->value == static_cast<const BoolLiteral*>(b)->value;
    case NodeKind::StringLiteral: return static_cast<const StringLiteral*>(a)->value == static_cast<const StringLiteral*>(b)->value;
    case NodeKind::Name: return static_cast<const Name*>(a)->id == static_cast<const Name*>(b)->id;
    case NodeKind::UnaryExpr: return static_cast<const Unary*>(a)->op == static_cast<const Unary*>(b)->op && equalExpr(static_cast<const Unary*>(a)->operand.get(), static_cast<const Unary*>(b)->operand.get());
    case NodeKind::BinaryExpr: return static_cast<const Binary*>(a)->op == static_cast<const Binary*>(b)->op && equalExpr(static_cast<const Binary*>(a)->lhs.get(), static_cast<const Binary*>(b)->lhs.get()) && equalExpr(static_cast<const Binary*>(a)->rhs.get(), static_cast<const Binary*>(b)->rhs.get());
    case NodeKind::TupleLiteral: {
      auto* ta = static_cast<const TupleLiteral*>(a); auto* tb = static_cast<const TupleLiteral*>(b);
      if (ta->elements.size() != tb->elements.size()) return false;
      for (size_t i = 0; i < ta->elements.size(); ++i) if (!equalExpr(ta->elements[i].get(), tb->elements[i].get())) return false; return true;
    }
    case NodeKind::ListLiteral: {
      auto* la = static_cast<const ListLiteral*>(a); auto* lb = static_cast<const ListLiteral*>(b);
      if (la->elements.size() != lb->elements.size()) return false;
      for (size_t i = 0; i < la->elements.size(); ++i) if (!equalExpr(la->elements[i].get(), lb->elements[i].get())) return false; return true;
    }
    default: return false;
  }
}

// Count pure subexpressions and rewrite duplicates by introducing a temp before the statement.
static std::size_t cseSubexprInStmt(std::vector<std::unique_ptr<Stmt>>& body, std::vector<std::unique_ptr<Stmt>>::iterator& it, int& tempIdx, std::unordered_set<std::string>& nameSet) {
  std::unordered_map<std::string, int> counts;
  std::unordered_map<std::string, const Expr*> exemplar;
  // Recursive walk to populate counts
  struct Walker : public ast::VisitorBase {
    std::unordered_map<std::string,int>& counts;
    std::unordered_map<std::string,const Expr*>& exemplar;
    explicit Walker(std::unordered_map<std::string,int>& c, std::unordered_map<std::string,const Expr*>& e) : counts(c), exemplar(e) {}
    void add(const Expr* e) {
      if (!EffectAlias::isPureExpr(e)) return;
      std::string key; hashExpr(e, key);
      counts[key]++;
      if (!exemplar.count(key)) exemplar[key] = e;
    }
    void visit(const Module&) override {}
    void visit(const FunctionDef&) override {}
    void visit(const ReturnStmt&) override {}
    void visit(const AssignStmt&) override {}
    void visit(const IfStmt&) override {}
    void visit(const ExprStmt& es) override { if (es.value) es.value->accept(*this); }
    void visit(const IntLiteral& n) override { add(&n); }
    void visit(const FloatLiteral& n) override { add(&n); }
    void visit(const BoolLiteral& n) override { add(&n); }
    void visit(const StringLiteral& n) override { add(&n); }
    void visit(const Name& n) override { add(&n); }
    void visit(const NoneLiteral&) override {}
    void visit(const Call&) override {}
    void visit(const Unary& u) override { add(&u); if (u.operand) u.operand->accept(*this); }
    void visit(const Binary& b) override { add(&b); if (b.lhs) b.lhs->accept(*this); if (b.rhs) b.rhs->accept(*this); }
    void visit(const TupleLiteral& t) override { add(&t); for (const auto& el : t.elements) if (el) el->accept(*this); }
    void visit(const ListLiteral& l) override { add(&l); for (const auto& el : l.elements) if (el) el->accept(*this); }
    void visit(const ObjectLiteral&) override {}
  };
  Walker w{counts, exemplar};
  if ((*it)->kind == NodeKind::ExprStmt) { (*it)->accept(w); }
  else if ((*it)->kind == NodeKind::AssignStmt) { auto* as = static_cast<AssignStmt*>(it->get()); if (as->value) as->value->accept(w); }
  // Pick a candidate with count>=2 and complexity>1
  std::string candKey; const Expr* candExpr = nullptr; int bestComplex = 0;
  for (const auto& kv : counts) {
    if (kv.second < 2) continue;
    const int comp = exprComplexity(exemplar[kv.first]);
    if (comp > 1 && comp > bestComplex) { bestComplex = comp; candKey = kv.first; candExpr = exemplar[kv.first]; }
  }
  if (!candExpr) return 0;
  // Generate unique temp name
  std::string tempName;
  do { tempName = std::string("_cse") + std::to_string(tempIdx++); } while (nameSet.count(tempName));
  nameSet.insert(tempName);
  // Insert temp assignment before current statement
  auto tempAssign = std::make_unique<AssignStmt>(tempName, cloneExpr(candExpr));
  auto insertPos = it - body.begin();
  body.insert(body.begin() + static_cast<long>(insertPos), std::move(tempAssign));
  // Insertion may invalidate iterators; re-establish iterator pointing to the original statement
  it = body.begin() + static_cast<long>(insertPos + 1);
  // Rewrite occurrences after the first
  struct Replacer : public ast::VisitorBase {
    const std::string& key; const Expr* exemplar; const std::string& tempName; int seen{0}; std::unique_ptr<Expr>* slot{nullptr};
    Replacer(const std::string& k, const Expr* ex, const std::string& t) : key(k), exemplar(ex), tempName(t) {}
    void replace(std::unique_ptr<Expr>& e) { if (!e) return; slot = &e; e->accept(*this); }
    void visit(const Module&) override {}
    void visit(const FunctionDef&) override {}
    void visit(const ReturnStmt&) override {}
    void visit(const AssignStmt&) override {}
    void visit(const IfStmt&) override {}
    void visit(const ExprStmt& es) override { if (es.value) replace(const_cast<std::unique_ptr<Expr>&>(es.value)); }
    void visit(const IntLiteral&) override {}
    void visit(const FloatLiteral&) override {}
    void visit(const BoolLiteral&) override {}
    void visit(const StringLiteral&) override {}
    void visit(const Name&) override {}
    void visit(const NoneLiteral&) override {}
    void visit(const Call&) override {}
    void visit(const Unary& u) override {
      auto* un = const_cast<Unary*>(&u);
      if (EffectAlias::isPureExpr(un)) {
        std::string k; hashExpr(un, k);
        if (k == key && equalExpr(un, exemplar)) {
          if (seen++ >= 1) { auto rep = std::make_unique<Name>(tempName); rep->line = un->line; rep->col = un->col; rep->file = un->file; *slot = std::move(rep); return; }
        }
      }
      if (un->operand) replace(un->operand);
    }
    void visit(const Binary& b) override {
      auto* bn = const_cast<Binary*>(&b);
      if (EffectAlias::isPureExpr(bn)) {
        std::string k; hashExpr(bn, k);
        if (k == key && equalExpr(bn, exemplar)) {
          if (seen++ >= 1) { auto rep = std::make_unique<Name>(tempName); rep->line = bn->line; rep->col = bn->col; rep->file = bn->file; *slot = std::move(rep); return; }
        }
      }
      if (bn->lhs) replace(bn->lhs); if (bn->rhs) replace(bn->rhs);
    }
    void visit(const TupleLiteral& t) override { auto* tp = const_cast<TupleLiteral*>(&t); for (auto& el : tp->elements) replace(el); }
    void visit(const ListLiteral& l) override { auto* li = const_cast<ListLiteral*>(&l); for (auto& el : li->elements) replace(el); }
    void visit(const ObjectLiteral&) override {}
  };
  Replacer rr{candKey, candExpr, tempName};
  if ((*it)->kind == NodeKind::ExprStmt) { (*it)->accept(rr); }
  else if ((*it)->kind == NodeKind::AssignStmt) { auto* as = static_cast<AssignStmt*>(it->get()); if (as->value) rr.replace(as->value); }
  return 1;
}

static std::size_t cseSubexprOnBlock(std::vector<std::unique_ptr<Stmt>>& body) {
  std::size_t changes = 0; int tempIdx = 0;
  // Collect existing names to avoid collisions
  std::unordered_set<std::string> names;
  for (const auto& s : body) {
    if (!s) continue;
    if (s->kind == NodeKind::AssignStmt) {
      const auto* as = static_cast<const AssignStmt*>(s.get());
      if (!as->targets.empty()) {
        for (const auto& t : as->targets) if (t && t->kind == NodeKind::Name) names.insert(static_cast<const Name*>(t.get())->id);
      } else if (!as->target.empty()) { names.insert(as->target); }
    }
  }
  for (auto it = body.begin(); it != body.end(); ++it) {
    // Iterate and apply at most one subexpr rewrite per statement per pass for simplicity
    changes += cseSubexprInStmt(body, it, tempIdx, names);
  }
  return changes;
}
} // namespace

std::size_t CSE::run(Module& module) {
  std::size_t total = 0;
  for (auto& fn : module.functions) {
    total += runOnBlock(fn->body);      // remove duplicate pure ExprStmts
    total += cseSubexprOnBlock(fn->body); // intra-statement subexpr rewriting with temps
  }
  return total;
}

} // namespace pycc::opt
