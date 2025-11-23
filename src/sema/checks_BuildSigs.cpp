/**
 * @file
 * @brief buildSigs: Extract function signatures from the module.
 */
#include "sema/detail/checks/BuildSigs.h"

namespace pycc::sema::detail {

void buildSigs(const ast::Module& mod,
               std::unordered_map<std::string, Sig>& outSigs) {
  outSigs.clear();
  for (const auto& func : mod.functions) {
    if (!func) continue;
    Sig sig; sig.ret = func->returnType;
    for (const auto& param : func->params) {
      sig.params.push_back(param.type);
      SigParam sp;
      sp.name = param.name;
      sp.type = param.type;
      sp.isVarArg = param.isVarArg;
      sp.isKwVarArg = param.isKwVarArg;
      sp.isKwOnly = param.isKwOnly;
      sp.isPosOnly = param.isPosOnly;
      sp.hasDefault = (param.defaultValue != nullptr);
      if (!param.unionTypes.empty()) {
        uint32_t m = 0U; for (auto ut : param.unionTypes) { m |= TypeEnv::maskForKind(ut); }
        sp.unionMask = m;
      }
      if (param.type == ast::TypeKind::List && param.listElemType != ast::TypeKind::NoneType) {
        sp.listElemMask = TypeEnv::maskForKind(param.listElemType);
      }
      sig.full.push_back(std::move(sp));
    }
    outSigs[func->name] = std::move(sig);
  }
}

} // namespace pycc::sema::detail

