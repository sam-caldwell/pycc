/***
 * Name: pycc::opt::SSABuilder (impl)
 */
#include "optimizer/SSABuilder.h"

#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/WhileStmt.h"
#include "ast/ForStmt.h"
#include "ast/TryStmt.h"
#include "ast/ReturnStmt.h"
#include <functional>
#include "ast/AssignStmt.h"
#include "ast/Name.h"
#include <iostream>

namespace pycc::opt {
using namespace pycc::ast;

namespace {
[[maybe_unused]] static bool isTerminator(const Stmt* s) {
  if (!s) return false;
  return (s->kind == NodeKind::ReturnStmt || s->kind == NodeKind::RaiseStmt);
}
[[maybe_unused]] static bool isSplit(const Stmt* s) {
  if (!s) return false;
  switch (s->kind) {
    case NodeKind::IfStmt:
    case NodeKind::WhileStmt:
    case NodeKind::ForStmt:
    case NodeKind::TryStmt:
      return true;
    default: return false;
  }
}
}

SSAFunction SSABuilder::build(FunctionDef& fn) {
  SSAFunction f; f.fn = &fn;
  int nextId = 0;

  auto newBlock = [&]() -> int {
    SSABlock b; b.id = nextId++; f.blocks.push_back(std::move(b)); return nextId - 1;
  };
  auto hasEdge = [&](int from, int to) -> bool {
    for (int s : f.blocks[from].succ) if (s == to) return true; return false;
  };
  auto connect = [&](int from, int to) {
    if (from < 0 || to < 0) return;
    if (!hasEdge(from, to)) { f.blocks[from].succ.push_back(to); f.blocks[to].pred.push_back(from); }
  };
  auto placeStmtInNewBlock = [&](Stmt* s, const std::vector<int>& preds) -> int {
    const int b = newBlock();
    f.blocks[b].stmts.push_back(s);
    f.blockOf[s] = b;
    for (int p : preds) connect(p, b);
    return b;
  };

  // Recursive builders return a vector of exit blocks for fallthrough paths
  std::function<std::vector<int>(std::vector<std::unique_ptr<Stmt>>&, const std::vector<int>&)> buildList;
  std::function<std::vector<int>(Stmt*, const std::vector<int>&)> buildStmt;

  buildStmt = [&](Stmt* s, const std::vector<int>& ins) -> std::vector<int> {
    if (!s) return ins;
    switch (s->kind) {
      case NodeKind::IfStmt: {
        auto* is = static_cast<IfStmt*>(s);
        // Condition block holding the IfStmt node
        const int condB = placeStmtInNewBlock(s, ins);
        // then-branch
        std::vector<int> thenIns{condB};
        auto thenOuts = buildList(is->thenBody, thenIns);
        if (thenOuts.empty()) thenOuts.push_back(condB);
        // else-branch
        std::vector<int> elseIns{condB};
        auto elseOuts = buildList(is->elseBody, elseIns);
        if (elseOuts.empty()) elseOuts.push_back(condB);
        // Join
        const int joinB = newBlock();
        for (int t : thenOuts) connect(t, joinB);
        for (int e : elseOuts) connect(e, joinB);
        return {joinB};
      }
      case NodeKind::WhileStmt: {
        auto* ws = static_cast<WhileStmt*>(s);
        // Header/cond block
        const int head = placeStmtInNewBlock(s, ins);
        // Body
        std::vector<int> bodyIns{head};
        auto bodyOuts = buildList(ws->thenBody, bodyIns);
        if (bodyOuts.empty()) bodyOuts.push_back(head);
        // Back-edges to header
        for (int b : bodyOuts) connect(b, head);
        // Exit/follow block
        const int follow = newBlock();
        connect(head, follow);
        // Optional else-body runs on loop exit
        if (!ws->elseBody.empty()) {
          auto elseOuts = buildList(ws->elseBody, {follow});
          if (elseOuts.empty()) return {follow};
          // Merge else outs into a join to keep single fallthrough
          const int joinB = newBlock();
          for (int e : elseOuts) connect(e, joinB);
          return {joinB};
        }
        return {follow};
      }
      case NodeKind::ForStmt: {
        auto* fs = static_cast<ForStmt*>(s);
        // Header representing iteration (acts like cond/latch)
        const int head = placeStmtInNewBlock(s, ins);
        // Body
        std::vector<int> bodyIns{head};
        auto bodyOuts = buildList(fs->thenBody, bodyIns);
        if (bodyOuts.empty()) bodyOuts.push_back(head);
        // Back-edges to header
        for (int b : bodyOuts) connect(b, head);
        // Exit/follow block
        const int follow = newBlock();
        connect(head, follow);
        if (!fs->elseBody.empty()) {
          auto elseOuts = buildList(fs->elseBody, {follow});
          if (elseOuts.empty()) return {follow};
          const int joinB = newBlock();
          for (int e : elseOuts) connect(e, joinB);
          return {joinB};
        }
        return {follow};
      }
      case NodeKind::TryStmt: {
        // Model as a single block and linear fallthrough for now
        const int b = placeStmtInNewBlock(s, ins);
        return {b};
      }
      case NodeKind::ReturnStmt:
      case NodeKind::RaiseStmt: {
        // Place terminator in its own block; no fallthrough
        (void)placeStmtInNewBlock(s, ins);
        return {};
      }
      default: {
        // Simple statement in its own block
        const int b = placeStmtInNewBlock(s, ins);
        return {b};
      }
    }
  };

  buildList = [&](std::vector<std::unique_ptr<Stmt>>& list, const std::vector<int>& ins) -> std::vector<int> {
    std::vector<int> curIns = ins;
    for (auto& st : list) {
      if (curIns.empty()) break; // no more reachable entries
      curIns = buildStmt(st.get(), curIns);
    }
    return curIns;
  };

  // Entry block
  const int entry = newBlock();
  (void)entry;
  // Build function body
  auto outs = buildList(fn.body, {entry});
  if (outs.empty()) {
    // Ensure at least one sink to keep CFG connected
    const int sink = newBlock();
    connect(entry, sink);
  } else if (outs.size() > 1) {
    // Join multiple exits to a single fallthrough to simplify downstream dominators
    const int joinB = newBlock();
    for (int b : outs) connect(b, joinB);
  }

  // Debug dump CFG if requested
  if (std::getenv("PYCC_SSA_GVN_DEBUG") != nullptr) {
    std::cerr << "[SSABuilder] CFG blocks: " << f.blocks.size() << "\n";
    for (const auto& bb : f.blocks) {
      std::cerr << "  B" << bb.id << ": pred=";
      for (size_t i=0;i<bb.pred.size();++i){ std::cerr << bb.pred[i] << (i+1<bb.pred.size()?",":""); }
      std::cerr << " succ=";
      for (size_t i=0;i<bb.succ.size();++i){ std::cerr << bb.succ[i] << (i+1<bb.succ.size()?",":""); }
      std::cerr << " stmts=" << bb.stmts.size() << "\n";
    }
  }
  // Populate defs (simple: names assigned in this block)
  for (auto& bb : f.blocks) {
    for (auto* s : bb.stmts) {
      if (!s) continue;
      if (s->kind == NodeKind::AssignStmt) {
        auto* as = static_cast<AssignStmt*>(s);
        if (!as->targets.empty()) {
          for (const auto& t : as->targets) if (t && t->kind == NodeKind::Name) bb.defs.insert(static_cast<Name*>(t.get())->id);
        } else if (!as->target.empty()) { bb.defs.insert(as->target); }
      }
    }
  }
  // Add phi placeholders at join points for vars defined in multiple preds
  for (auto& bb : f.blocks) {
    if (bb.pred.size() < 2) continue;
    std::unordered_map<std::string,int> count;
    for (int p : bb.pred) {
      if (p < 0 || p >= static_cast<int>(f.blocks.size())) continue;
      for (const auto& v : f.blocks[p].defs) count[v]++;
    }
    for (const auto& kv : count) {
      if (kv.second >= 2) { SSABlock::SSAPhi phi; phi.var = kv.first; phi.incomings = bb.pred; bb.phis.push_back(std::move(phi)); }
    }
  }
  return f;
}

namespace {
// Helper: intersect two sets represented as unordered_set<int>
static std::unordered_set<int> set_intersect(const std::unordered_set<int>& a, const std::unordered_set<int>& b) {
  std::unordered_set<int> r; if (a.size() < b.size()) {
    for (int x : a) if (b.count(x)) r.insert(x);
  } else {
    for (int x : b) if (a.count(x)) r.insert(x);
  } return r;
}
}

SSABuilder::DomTree SSABuilder::computeDominators(const SSAFunction& fn) const {
  const int N = static_cast<int>(fn.blocks.size());
  DomTree dt; dt.idom.assign(N, -1); dt.children.assign(N, {});
  if (N == 0) return dt;
  // Initialize dom sets
  std::vector<std::unordered_set<int>> dom(N);
  std::unordered_set<int> all;
  for (int i = 0; i < N; ++i) all.insert(i);
  dom[0].insert(0);
  for (int i = 1; i < N; ++i) dom[i] = all;
  bool changed = true;
  while (changed) {
    changed = false;
    for (int n = 1; n < N; ++n) {
      auto newdom = all;
      if (fn.blocks[n].pred.empty()) newdom = {};
      else {
        bool first = true;
        for (int p : fn.blocks[n].pred) {
          if (p < 0 || p >= N) continue;
          if (first) { newdom = dom[p]; first = false; }
          else { newdom = set_intersect(newdom, dom[p]); }
        }
      }
      newdom.insert(n);
      if (newdom != dom[n]) { dom[n] = std::move(newdom); changed = true; }
    }
  }
  // Compute idom from dom sets
  for (int n = 1; n < N; ++n) {
    // candidates = dom[n] - {n}
    std::vector<int> cand; cand.reserve(dom[n].size());
    for (int d : dom[n]) if (d != n) cand.push_back(d);
    int best = -1;
    for (int d : cand) {
      bool dominatedByAllOthers = true;
      for (int e : cand) { if (e == d) continue; if (!dom[d].count(e)) { dominatedByAllOthers = false; break; } }
      if (dominatedByAllOthers) { best = d; break; }
    }
    dt.idom[n] = best;
  }
  for (int n = 1; n < N; ++n) { int id = dt.idom[n]; if (id >= 0) dt.children[id].push_back(n); }
  return dt;
}

} // namespace pycc::opt
