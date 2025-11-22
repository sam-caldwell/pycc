/***
 * Name: pycc::opt::RangeAnalysis (impl)
 */
#include "optimizer/RangeAnalysis.h"

#include "ast/Module.h"
#include "ast/FunctionDef.h"
#include "ast/AssignStmt.h"
#include "ast/Name.h"
#include "ast/IntLiteral.h"

namespace pycc::opt {
using namespace pycc::ast;

std::unordered_map<std::string, RangeAnalysis::Range> RangeAnalysis::analyze(const Module& module) {
  std::unordered_map<std::string, Range> out;
  for (const auto& fn : module.functions) {
    for (const auto& st : fn->body) {
      if (!st || st->kind != NodeKind::AssignStmt) continue;
      const auto* asg = static_cast<const AssignStmt*>(st.get());
      const Name* lhs = nullptr;
      if (!asg->targets.empty() && asg->targets.size() == 1 && asg->targets[0] && asg->targets[0]->kind == NodeKind::Name) {
        lhs = static_cast<const Name*>(asg->targets[0].get());
      } else if (asg->targets.empty() && !asg->target.empty()) {
        static Name tmp(""); // not used
        lhs = &tmp; const_cast<Name*>(lhs)->id = asg->target;
      }
      if (!lhs || !asg->value || asg->value->kind != NodeKind::IntLiteral) continue;
      const auto v = static_cast<const IntLiteral*>(asg->value.get())->value;
      auto it = out.find(lhs->id);
      if (it == out.end()) { out.emplace(lhs->id, Range{v, v}); }
      else { it->second.min = std::min(it->second.min, v); it->second.max = std::max(it->second.max, v); }
    }
  }
  return out;
}

size_t RangeAnalysis::run(Module& m) {
  (void)analyze(m);
  return 0; // analysis only
}

} // namespace pycc::opt
