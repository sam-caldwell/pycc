/***
 * Name: pycc::opt::DCE (impl)
 */
#include "optimizer/DCE.h"

namespace pycc::opt {
using namespace pycc::ast;

static size_t dceBlock(std::vector<std::unique_ptr<Stmt>>& body) {
  size_t removed = 0;
  std::vector<std::unique_ptr<Stmt>> newBody;
  bool seenReturn = false;
  for (auto& st : body) {
    if (seenReturn) { ++removed; continue; }
    if (st->kind == NodeKind::ReturnStmt) {
      seenReturn = true;
      newBody.emplace_back(std::move(st));
      continue;
    }
    if (st->kind == NodeKind::IfStmt) {
      auto* i = static_cast<IfStmt*>(st.get());
      removed += dceBlock(i->thenBody);
      removed += dceBlock(i->elseBody);
    }
    newBody.emplace_back(std::move(st));
  }
  body = std::move(newBody);
  return removed;
}

size_t DCE::run(Module& m) {
  stats_.clear(); size_t removed = 0;
  for (auto& fn : m.functions) {
    removed += dceBlock(fn->body);
  }
  stats_["removed"] = removed;
  return removed;
}

} // namespace pycc::opt

