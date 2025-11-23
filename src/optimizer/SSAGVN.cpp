/***
 * Name: pycc::opt::SSAGVN (impl)
 */
#include "optimizer/SSAGVN.h"

#include "optimizer/SSABuilder.h"
#include "optimizer/EffectAlias.h"
#include "ast/Module.h"
#include "ast/FunctionDef.h"
#include "ast/AssignStmt.h"
#include "ast/ExprStmt.h"
#include "ast/Unary.h"
#include "ast/Binary.h"
#include "ast/TupleLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/Attribute.h"
#include "ast/Subscript.h"
#include "ast/IntLiteral.h"
#include "ast/FloatLiteral.h"
#include "ast/BoolLiteral.h"
#include "ast/StringLiteral.h"
#include "ast/Name.h"
#include <unordered_map>
#include <functional>

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
    case NodeKind::Name: out += static_cast<const Name*>(e)->id; break;
    case NodeKind::UnaryExpr: hashExpr(static_cast<const Unary*>(e)->operand.get(), out); break;
    case NodeKind::BinaryExpr: { const auto* b = static_cast<const Binary*>(e); out += std::to_string(static_cast<int>(b->op)); hashExpr(b->lhs.get(), out); hashExpr(b->rhs.get(), out); break; }
    case NodeKind::TupleLiteral: { const auto* t = static_cast<const TupleLiteral*>(e); for (const auto& el : t->elements) hashExpr(el.get(), out); break; }
    case NodeKind::ListLiteral: { const auto* l = static_cast<const ListLiteral*>(e); for (const auto& el : l->elements) hashExpr(el.get(), out); break; }
    case NodeKind::Attribute: { const auto* a = static_cast<const Attribute*>(e); hashExpr(a->value.get(), out); out += '.'; out += a->attr; break; }
    case NodeKind::Subscript: { const auto* s = static_cast<const Subscript*>(e); hashExpr(s->value.get(), out); out.push_back('['); hashExpr(s->slice.get(), out); out.push_back(']'); break; }
    default: out += "?"; break;
  }
}

static void replaceExpr(std::unique_ptr<Expr>& e, const std::string& name) {
  auto rep = std::make_unique<Name>(name);
  if (e) { rep->line = e->line; rep->col = e->col; rep->file = e->file; }
  e = std::move(rep);
}

// Rewrite subexpressions within a block using value numbering table; conservative dominance: only within block
// and across single-predecessor sequences (not full dom tree).
static bool isCSENamed(const std::string& name) { return name.rfind("_cse", 0) == 0; }

static std::size_t gvnBlock(SSABlock& bb,
                             std::unordered_map<std::string,std::string>& valTable,
                             const std::unordered_map<std::string,int>& writeCount) {
  std::size_t changes = 0;
  struct Rewriter : public ast::VisitorBase {
    std::unordered_map<std::string,std::string>& valTable;
    std::size_t& changes;
    std::unique_ptr<Expr>* slot{nullptr};
    explicit Rewriter(std::unordered_map<std::string,std::string>& vt, std::size_t& ch) : valTable(vt), changes(ch) {}
    void rw(std::unique_ptr<Expr>& e) { if (!e) return; slot = &e; e->accept(*this); }
    // Required pure virtual stubs
    void visit(const Module&) override {}
    void visit(const FunctionDef&) override {}
    void visit(const ReturnStmt&) override {}
    void visit(const AssignStmt&) override {}
    void visit(const IfStmt&) override {}
    void visit(const ExprStmt&) override {}
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
        auto it = valTable.find(k);
        if (it != valTable.end()) { replaceExpr(*slot, it->second); ++changes; return; }
      }
      if (un->operand) rw(un->operand);
    }
    void visit(const Binary& b) override {
      auto* bn = const_cast<Binary*>(&b);
      if (EffectAlias::isPureExpr(bn)) {
        std::string k; hashExpr(bn, k);
        auto it = valTable.find(k);
        if (it != valTable.end()) { replaceExpr(*slot, it->second); ++changes; return; }
      }
      if (bn->lhs) rw(bn->lhs); if (bn->rhs) rw(bn->rhs);
    }
    void visit(const TupleLiteral& t) override { auto* tp = const_cast<TupleLiteral*>(&t); for (auto& el : tp->elements) rw(el); }
    void visit(const ListLiteral& l) override { auto* li = const_cast<ListLiteral*>(&l); for (auto& el : li->elements) rw(el); }
    void visit(const Attribute& a) override {
      auto* at = const_cast<Attribute*>(&a);
      if (EffectAlias::isPureExpr(at)) {
        std::string k; hashExpr(at, k);
        auto it = valTable.find(k);
        if (it != valTable.end()) { replaceExpr(*slot, it->second); ++changes; return; }
      }
      if (at->value) rw(at->value);
    }
    void visit(const Subscript& s) override {
      auto* sb = const_cast<Subscript*>(&s);
      if (EffectAlias::isPureExpr(sb)) {
        std::string k; hashExpr(sb, k);
        auto it = valTable.find(k);
        if (it != valTable.end()) { replaceExpr(*slot, it->second); ++changes; return; }
      }
      if (sb->value) rw(sb->value); if (sb->slice) rw(sb->slice);
    }
    void visit(const ObjectLiteral&) override {}
  };

  // Collect exprs into table when assigned to names
  for (auto* s : bb.stmts) {
    if (!s) continue;
    if (s->kind == NodeKind::AssignStmt) {
      auto* as = static_cast<AssignStmt*>(s);
      if (as->value && EffectAlias::isPureExpr(as->value.get())) {
        std::string k; hashExpr(as->value.get(), k);
        // map to the LHS name if simple
        std::string lhs;
        if (!as->targets.empty() && as->targets.size() == 1 && as->targets[0] && as->targets[0]->kind == NodeKind::Name) {
          lhs = static_cast<Name*>(as->targets[0].get())->id;
        } else if (as->targets.empty() && !as->target.empty()) { lhs = as->target; }
        // Only propagate names that are single-assignment in the function to ensure safety across blocks.
        if (!lhs.empty() && !k.empty()) {
          auto itc = writeCount.find(lhs);
          if (itc != writeCount.end() && itc->second == 1) {
            auto existing = valTable.find(k);
            if (existing == valTable.end()) valTable.emplace(k, lhs);
            else if (!isCSENamed(existing->second) && isCSENamed(lhs)) existing->second = lhs; // prefer CSE temps
          }
        }
      }
      // Also rewrite RHS via table
      if (as->value) { Rewriter rw{valTable, changes}; rw.rw(as->value); }
    } else if (s->kind == NodeKind::ExprStmt) {
      auto* es = static_cast<ExprStmt*>(s);
      if (es->value) { Rewriter rw{valTable, changes}; rw.rw(es->value); }
    }
  }
  return changes;
}
} // namespace

std::size_t SSAGVN::run(Module& module) {
  std::size_t total = 0;
  SSABuilder builder;
  for (auto& fn : module.functions) {
    auto ssa = builder.build(*fn);
    auto dom = builder.computeDominators(ssa);
    // Compute writes per name to restrict cross-block reuse to single-assignment names
    std::unordered_map<std::string,int> writes;
    for (const auto& bb : ssa.blocks) {
      for (const auto* s : bb.stmts) {
        if (!s || s->kind != NodeKind::AssignStmt) continue;
        const auto* as = static_cast<const AssignStmt*>(s);
        if (!as->targets.empty()) {
          for (const auto& t : as->targets) if (t && t->kind == NodeKind::Name) writes[static_cast<const Name*>(t.get())->id]++;
        } else if (!as->target.empty()) { writes[as->target]++; }
      }
    }
    // If any variable in the function has multiple writes, skip GVN for safety in this function.
    bool hasMultiWrite = false;
    for (const auto &kv : writes) { if (kv.second > 1) { hasMultiWrite = true; break; } }
    if (hasMultiWrite) { continue; }
    // Prepare per-block out tables
    std::vector<std::unordered_map<std::string,std::string>> out(ssa.blocks.size());
    // Traverse dominator tree starting at entry
    std::function<void(int)> walk = [&](int b) {
      std::unordered_map<std::string,std::string> valTable;
      int id = (b >= 0 && b < static_cast<int>(dom.idom.size())) ? dom.idom[b] : -1;
      if (id >= 0) {
        // inherit from idom, filter to single-assignment
        for (const auto& kv : out[id]) {
          const auto& name = kv.second;
          auto itc = writes.find(name);
          if (itc != writes.end() && itc->second == 1) valTable.emplace(kv.first, kv.second);
        }
      }
      total += gvnBlock(ssa.blocks[b], valTable, writes);
      out[b] = std::move(valTable);
      for (int c : dom.children[b]) walk(c);
    };
    if (!ssa.blocks.empty()) walk(0);
  }
  return total;
}

} // namespace pycc::opt
