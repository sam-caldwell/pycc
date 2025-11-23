/**
 * @file
 * @brief resolveNamedCall: Resolve calls where callee is a Name using signature map.
 */
#include "sema/detail/exptyper/CallResolve.h"
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/helpers/AddDiag.h"

namespace pycc::sema::detail {

bool resolveNamedCall(const ast::Call& callNode,
                      const ast::Name* calleeName,
                      const TypeEnv& env,
                      const std::unordered_map<std::string, Sig>& sigs,
                      const std::unordered_map<std::string, int>& retParamIdxs,
                      std::vector<Diagnostic>& diags,
                      PolyPtrs polyTargets,
                      const std::vector<const TypeEnv*>* outers,
                      const std::unordered_map<std::string, ClassInfo>* /*classes*/,
                      ast::TypeKind& out,
                      uint32_t& outSet,
                      bool& ok) {
  (void)outSet;
  auto it = sigs.find(calleeName->id);
  if (it == sigs.end()) return false;
  const Sig& sig = it->second;
  if (!sig.full.empty()) {
    std::vector<size_t> posParamIdxs; posParamIdxs.reserve(sig.full.size()); size_t varargIdx = static_cast<size_t>(-1), kwvarargIdx = static_cast<size_t>(-1);
    std::unordered_map<std::string, size_t> nameToIdx;
    for (size_t i = 0; i < sig.full.size(); ++i) { const auto& p = sig.full[i]; if (p.isVarArg) varargIdx = i; else if (p.isKwVarArg) kwvarargIdx = i; else { nameToIdx[p.name] = i; if (!p.isKwOnly) posParamIdxs.push_back(i); } }
    std::vector<bool> bound(sig.full.size(), false);
    // Positional args
    for (size_t i = 0; i < callNode.args.size(); ++i) {
      ExpressionTyper at{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[i]->accept(at); if (!at.ok) { ok = false; return true; }
      if (i < posParamIdxs.size()) { size_t pidx = posParamIdxs[i]; if (at.out != sig.full[pidx].type) { addDiag(diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return true; } bound[pidx] = true; }
      else if (varargIdx != static_cast<size_t>(-1)) { if (sig.full[varargIdx].type != ast::TypeKind::NoneType && at.out != sig.full[varargIdx].type) { addDiag(diags, "*args element type mismatch", callNode.args[i].get()); ok = false; return true; } }
      else { addDiag(diags, std::string("arity mismatch calling function: ") + calleeName->id, &callNode); ok = false; return true; }
    }
    // Keyword args
    for (const auto& kw : callNode.keywords) {
      auto itn = nameToIdx.find(kw.name);
      if (itn == nameToIdx.end()) { if (kwvarargIdx == static_cast<size_t>(-1)) { addDiag(diags, std::string("unknown keyword argument: ") + kw.name, &callNode); ok = false; return true; } continue; }
      size_t pidx = itn->second; if (sig.full[pidx].isPosOnly) { addDiag(diags, std::string("positional-only argument passed as keyword: ") + kw.name, &callNode); ok = false; return true; }
      if (bound[pidx]) { addDiag(diags, std::string("multiple values for argument: ") + kw.name, &callNode); ok = false; return true; }
      ExpressionTyper kt{env, sigs, retParamIdxs, diags, polyTargets, outers}; if (kw.value) kw.value->accept(kt); if (!kt.ok) { ok = false; return true; }
      // Enforce union/list annotations
      const auto& p = sig.full[pidx]; bool typeOk = false; const uint32_t aMask = TypeEnv::maskForKind(kt.out);
      if (p.unionMask != 0U) { typeOk = ((aMask & p.unionMask) != 0U); }
      else if (p.type == ast::TypeKind::List && p.listElemMask != 0U && kt.out == ast::TypeKind::List) { typeOk = true; }
      else { typeOk = (kt.out == p.type); }
      if (!typeOk) { addDiag(diags, std::string("keyword argument type mismatch: ") + kw.name, &callNode); ok = false; return true; }
      bound[pidx] = true;
    }
    // Final required params
    for (size_t i = 0; i < sig.full.size(); ++i) { const auto& sp = sig.full[i]; if (sp.isVarArg || sp.isKwVarArg) continue; if (!bound[i] && !sp.hasDefault) { addDiag(diags, std::string("missing required ") + (sp.isKwOnly ? "keyword-only argument: " : "positional argument: ") + sp.name, &callNode); ok = false; return true; } }
    out = sig.ret; return true;
  }
  // Simple signature
  if (sig.params.size() != callNode.args.size()) { addDiag(diags, std::string("arity mismatch calling function: ") + calleeName->id, &callNode); ok = false; return true; }
  for (size_t i = 0; i < callNode.args.size(); ++i) { ExpressionTyper at{env, sigs, retParamIdxs, diags, polyTargets, outers}; callNode.args[i]->accept(at); if (!at.ok) { ok = false; return true; } if (at.out != sig.params[i]) { addDiag(diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return true; } }
  out = sig.ret; return true;
}

} // namespace pycc::sema::detail

