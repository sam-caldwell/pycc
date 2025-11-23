/**
 * @file
 * @brief resolveAttributeCall: Resolve attribute calls like Class.method(...) via signatures.
 */
#include "sema/detail/exptyper/CallResolve.h"
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/helpers/AddDiag.h"

namespace pycc::sema::detail {

bool resolveAttributeCall(const ast::Call& callNode,
                          const ast::Attribute* at,
                          const TypeEnv& env,
                          const std::unordered_map<std::string, Sig>& sigs,
                          const std::unordered_map<std::string, int>& retParamIdxs,
                          std::vector<Diagnostic>& diags,
                          PolyPtrs polyTargets,
                          const std::vector<const TypeEnv*>* outers,
                          const std::unordered_map<std::string, ClassInfo>* classes,
                          ast::TypeKind& out,
                          uint32_t& outSet,
                          bool& ok) {
  (void)outSet; (void)classes;
  if (!at->value || at->value->kind != ast::NodeKind::Name) return false;
  const auto* nameNode = static_cast<const ast::Name*>(at->value.get());
  const std::string key = nameNode->id + std::string(".") + at->attr;
  auto it = sigs.find(key);
  if (it == sigs.end()) return false;
  const Sig& sig = it->second;
  if (!sig.full.empty()) {
    std::unordered_map<std::string, size_t> nameToIdx; std::vector<size_t> posIdxs; size_t varargIdx = static_cast<size_t>(-1), kwvarargIdx = static_cast<size_t>(-1);
    posIdxs.reserve(sig.full.size());
    for (size_t i = 0; i < sig.full.size(); ++i) { const auto& p = sig.full[i]; if (p.isVarArg) varargIdx = i; else if (p.isKwVarArg) kwvarargIdx = i; else { nameToIdx[p.name] = i; if (!p.isKwOnly) posIdxs.push_back(i); } }
    std::vector<bool> bound(sig.full.size(), false);
    for (size_t i = 0; i < callNode.args.size(); ++i) {
      ExpressionTyper at2{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[i]->accept(at2); if (!at2.ok) { ok = false; return true; }
      if (i < posIdxs.size()) { size_t pidx = posIdxs[i]; if (at2.out != sig.full[pidx].type) { addDiag(diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return true; } bound[pidx] = true; }
      else if (varargIdx != static_cast<size_t>(-1)) { if (sig.full[varargIdx].type != ast::TypeKind::NoneType && at2.out != sig.full[varargIdx].type) { addDiag(diags, "*args element type mismatch", callNode.args[i].get()); ok = false; return true; } }
      else { addDiag(diags, std::string("arity mismatch calling function: ") + (nameNode->id + std::string(".__call__")), &callNode); ok = false; return true; }
    }
    for (const auto& kw : callNode.keywords) {
      auto itn = nameToIdx.find(kw.name);
      if (itn == nameToIdx.end()) { if (kwvarargIdx == static_cast<size_t>(-1)) { addDiag(diags, std::string("unknown keyword argument: ") + kw.name, &callNode); ok = false; return true; } continue; }
      size_t pidx = itn->second; if (bound[pidx]) { addDiag(diags, std::string("multiple values for argument: ") + kw.name, &callNode); ok = false; return true; }
      ExpressionTyper kt{env, sigs, retParamIdxs, diags, polyTargets, outers}; if (kw.value) kw.value->accept(kt); if (!kt.ok) { ok = false; return true; }
      if (kt.out != sig.full[pidx].type) { addDiag(diags, std::string("keyword argument type mismatch: ") + kw.name, &callNode); ok = false; return true; }
      bound[pidx] = true;
    }
    out = sig.ret; return true;
  }
  if (sig.params.size() != callNode.args.size()) { addDiag(diags, std::string("arity mismatch calling function: ") + key, &callNode); ok = false; return true; }
  for (size_t i = 0; i < callNode.args.size(); ++i) { ExpressionTyper argTyper{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[i]->accept(argTyper); if (!argTyper.ok) { ok = false; return true; } if (argTyper.out != sig.params[i]) { addDiag(diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return true; } }
  out = sig.ret; return true;
}

} // namespace pycc::sema::detail

