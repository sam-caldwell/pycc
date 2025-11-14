/***
 * Name: pycc::opt::DCE (impl)
 */
#include "optimizer/DCE.h"

namespace pycc::opt {
using namespace pycc::ast;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static size_t dceBlock(std::vector<std::unique_ptr<Stmt>>& body) {
  size_t removed = 0;
  std::vector<std::unique_ptr<Stmt>> newBody;
  bool seenReturn = false; // NOLINT(misc-const-correctness)
  for (auto& st : body) {
    if (seenReturn) { ++removed; continue; }
    if (st->kind == NodeKind::ReturnStmt) {
      seenReturn = true;
      newBody.emplace_back(std::move(st));
      continue;
    }
    if (st->kind == NodeKind::IfStmt) {
      auto* ifStmt = static_cast<IfStmt*>(st.get());
      removed += dceBlock(ifStmt->thenBody);
      removed += dceBlock(ifStmt->elseBody);
    }
    newBody.emplace_back(std::move(st));
  }
  body = std::move(newBody);
  return removed;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
size_t DCE::run(Module& module) {
  stats_.clear();
  size_t removed = 0;
  for (auto& func : module.functions) {
    removed += dceBlock(func->body);
  }
  stats_["removed"] = removed;
  return removed;
}

} // namespace pycc::opt
