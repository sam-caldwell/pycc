/**
 * @file
 * @brief collectClasses: Build ClassInfo map from module classes and validate methods.
 */
#include "sema/detail/checks/CollectClasses.h"
#include "sema/detail/checks/ValidateClassMethod.h"
#include "ast/Name.h"
#include "ast/DefStmt.h"

namespace pycc::sema::detail {

void collectClasses(const ast::Module& mod,
                    std::unordered_map<std::string, ClassInfo>& out,
                    std::vector<Diagnostic>& diags) {
  out.clear();
  for (const auto& clsPtr : mod.classes) {
    if (!clsPtr) continue;
    ClassInfo ci;
    for (const auto& b : clsPtr->bases) {
      if (b && b->kind == ast::NodeKind::Name) {
        const auto* nm = static_cast<const ast::Name*>(b.get());
        ci.bases.push_back(nm->id);
      }
    }
    for (const auto& st : clsPtr->body) {
      if (!st || st->kind != ast::NodeKind::DefStmt) continue;
      const auto* ds = static_cast<const ast::DefStmt*>(st.get());
      if (!ds->func) continue;
      const auto* fn = ds->func.get();
      validateClassMethod(fn, clsPtr->name, diags);
      Sig sig; sig.ret = fn->returnType;
      for (const auto& p : fn->params) {
        sig.params.push_back(p.type);
        SigParam sp; sp.name = p.name; sp.type = p.type; sp.isVarArg = p.isVarArg; sp.isKwVarArg = p.isKwVarArg;
        sp.isKwOnly = p.isKwOnly; sp.isPosOnly = p.isPosOnly; sp.hasDefault = (p.defaultValue != nullptr);
        sig.full.push_back(std::move(sp));
      }
      ci.methods[fn->name] = std::move(sig);
    }
    out[clsPtr->name] = std::move(ci);
  }
}

} // namespace pycc::sema::detail
