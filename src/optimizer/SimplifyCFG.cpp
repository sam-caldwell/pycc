/***
 * Name: pycc::opt::SimplifyCFG (impl)
 */
#include "optimizer/SimplifyCFG.h"
#include "ast/IfStmt.h"
#include "ast/BoolLiteral.h"
#include <memory>
#include <utility>
#include <vector>

namespace pycc::opt {
using namespace pycc::ast;

static bool isConstBool(const Expr* e, bool& val) {
  if (e && e->kind == NodeKind::BoolLiteral) { val = static_cast<const BoolLiteral*>(e)->value; return true; }
  return false;
}
static void simplifyBlock(std::vector<std::unique_ptr<Stmt>>& body, size_t& pruned) {
  std::vector<std::unique_ptr<Stmt>> out;
  out.reserve(body.size());
  for (auto& s : body) {
    if (s->kind == NodeKind::IfStmt) {
      auto* ifs = static_cast<IfStmt*>(s.get());
      simplifyBlock(ifs->thenBody, pruned);
      simplifyBlock(ifs->elseBody, pruned);
      bool v = false;
      if (ifs->cond && isConstBool(ifs->cond.get(), v)) {
        if (v) { for (auto& t : ifs->thenBody) out.emplace_back(std::move(t)); }
        else { for (auto& e : ifs->elseBody) out.emplace_back(std::move(e)); }
        ++pruned;
        continue;
      }
      out.emplace_back(std::move(s));
    } else {
      out.emplace_back(std::move(s));
    }
  }
  body = std::move(out);
}

size_t SimplifyCFG::run(Module& module) {
  stats_.clear();
  size_t pruned = 0;
  for (auto& f : module.functions) {
    simplifyBlock(f->body, pruned);
  }
  stats_["pruned_if"] = pruned;
  return pruned;
}

} // namespace pycc::opt
