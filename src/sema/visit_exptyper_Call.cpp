/***
 * Name: ExpressionTyper::visit(Call)
 * Purpose: Type-check calls: stdlib fast-paths, function signatures, and polymorphism.
 */
#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/detail/Helpers.h"
#include "ast/Call.h"
#include "ast/Attribute.h"
#include "ast/Name.h"
#include "ast/ListLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/DictLiteral.h"

using namespace pycc;
using namespace pycc::sema;

// NOLINTNEXTLINE(readability-function-cognitive-complexity,readability-function-size)
void ExpressionTyper::visit(const ast::Call& callNode) {
  // Fast-path: stdlib modules dispatched by attribute on module name (avoid falling into unknown poly resolution)
  if (callNode.callee && callNode.callee->kind == ast::NodeKind::Attribute) {
    const auto* at0 = static_cast<const ast::Attribute*>(callNode.callee.get());
    if (at0->value && at0->value->kind == ast::NodeKind::Name) {
      const auto* base0 = static_cast<const ast::Name*>(at0->value.get());
      // math.* typed helpers
      if (base0->id == "math") {
        const std::string fn = at0->attr;
        auto checkUnary = [&](ast::TypeKind retKind) {
          if (callNode.args.size() != 1) { addDiag(*diags, std::string("math.") + fn + "() takes 1 arg", &callNode); ok = false; return; }
          ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
          const uint32_t mask = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
          const uint32_t okmask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float);
          if ((mask & ~okmask) != 0U) { addDiag(*diags, std::string("math.") + fn + ": argument must be int/float", callNode.args[0].get()); ok = false; return; }
          out = retKind; const_cast<ast::Call&>(callNode).setType(out);
        };
        auto checkBinary = [&](ast::TypeKind retKind) {
          if (callNode.args.size() != 2) { addDiag(*diags, std::string("math.") + fn + "() takes 2 args", &callNode); ok = false; return; }
          ExpressionTyper a0{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a0); if (!a0.ok) { ok = false; return; }
          ExpressionTyper a1{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[1]->accept(a1); if (!a1.ok) { ok = false; return; }
          const uint32_t okmask = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Float);
          const uint32_t m0 = (a0.outSet != 0U) ? a0.outSet : TypeEnv::maskForKind(a0.out);
          const uint32_t m1 = (a1.outSet != 0U) ? a1.outSet : TypeEnv::maskForKind(a1.out);
          if ((m0 & ~okmask) != 0U || (m1 & ~okmask) != 0U) { addDiag(*diags, std::string("math.") + fn + ": arguments must be int/float", &callNode); ok = false; return; }
          out = retKind; const_cast<ast::Call&>(callNode).setType(out);
        };
        if (fn == "sqrt" || fn == "fabs" || fn == "sin" || fn == "cos" || fn == "tan" || fn == "asin" || fn == "acos" || fn == "atan" || fn == "exp" || fn == "exp2" || fn == "log" || fn == "log2" || fn == "log10" || fn == "degrees" || fn == "radians") { checkUnary(ast::TypeKind::Float); return; }
        if (fn == "floor" || fn == "ceil" || fn == "trunc") { checkUnary(ast::TypeKind::Int); return; }
        if (fn == "pow" || fn == "copysign" || fn == "atan2" || fn == "fmod" || fn == "hypot") { checkBinary(ast::TypeKind::Float); return; }
      }
      if (base0->id == "subprocess") {
        const std::string fn = at0->attr;
        if (callNode.args.size() != 1) { addDiag(*diags, std::string("subprocess.") + fn + "() takes 1 arg", &callNode); ok = false; return; }
        ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
        const uint32_t mask = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
        const uint32_t strMask = TypeEnv::maskForKind(ast::TypeKind::Str);
        if ((mask & ~strMask) != 0U) { addDiag(*diags, std::string("subprocess.") + fn + ": argument must be str", callNode.args[0].get()); ok = false; return; }
        out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); return;
      }
      if (base0->id == "sys") {
        const std::string fn = at0->attr;
        if (fn == "exit") {
          if (callNode.args.size() != 1) { addDiag(*diags, "sys.exit() takes 1 arg", &callNode); ok = false; return; }
          ExpressionTyper a{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[0]->accept(a); if (!a.ok) { ok = false; return; }
          const uint32_t allow = TypeEnv::maskForKind(ast::TypeKind::Int) | TypeEnv::maskForKind(ast::TypeKind::Bool) | TypeEnv::maskForKind(ast::TypeKind::Float);
          const uint32_t m = (a.outSet != 0U) ? a.outSet : TypeEnv::maskForKind(a.out);
          if ((m & ~allow) != 0U) { addDiag(*diags, "sys.exit: int/bool/float required", callNode.args[0].get()); ok = false; return; }
          out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
        }
        if (fn == "platform" || fn == "version") { if (!callNode.args.empty()) { addDiag(*diags, std::string("sys.") + fn + "() takes 0 args", &callNode); ok = false; return; } out = ast::TypeKind::Str; const_cast<ast::Call&>(callNode).setType(out); return; }
        if (fn == "maxsize") { if (!callNode.args.empty()) { addDiag(*diags, "sys.maxsize() takes 0 args", &callNode); ok = false; return; } out = ast::TypeKind::Int; const_cast<ast::Call&>(callNode).setType(out); return; }
      }
    }
  }

  // General call typing
  // 1) Direct name: resolve against signatures and attribute polymorphism maps
  if (callNode.callee && callNode.callee->kind == ast::NodeKind::Name) {
    const auto* nameNode = static_cast<const ast::Name*>(callNode.callee.get());
    auto it = sigs->find(nameNode->id);
    if (it == sigs->end()) {
      // Could be a variable alias to multiple polymorphic targets
      if (polyTargets.vars != nullptr) {
        auto itp = polyTargets.vars->find(nameNode->id);
        if (itp != polyTargets.vars->end() && !itp->second.empty()) {
          // If multiple possibilities remain, this is ambiguous; accept conservatively as Any/NoneType
          out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return;
        }
      }
      addDiag(*diags, std::string("unknown function: ") + nameNode->id, &callNode); ok = false; return;
    }
    const Sig& sig = it->second;
    auto& mutableCall = const_cast<ast::Call&>(callNode);
    if (!sig.full.empty()) {
      // Map name->idx and positions; find vararg/kwvararg
      std::unordered_map<std::string, size_t> nameToIdx; std::vector<size_t> posParamIdxs; size_t varargIdx = static_cast<size_t>(-1), kwvarargIdx = static_cast<size_t>(-1);
      posParamIdxs.reserve(sig.full.size());
      for (size_t i = 0; i < sig.full.size(); ++i) {
        const auto& p = sig.full[i];
        if (p.isVarArg) { varargIdx = i; }
        else if (p.isKwVarArg) { kwvarargIdx = i; }
        else {
          nameToIdx[p.name] = i; if (!p.isKwOnly) posParamIdxs.push_back(i);
        }
      }
      // Bound set
      std::vector<bool> bound(sig.full.size(), false);
      // Resolve any star-args first to ensure they meet annotation constraints
      if (!callNode.starArgs.empty()) {
        for (const auto& sa : callNode.starArgs) {
          if (!sa) continue; ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets}; sa->accept(at); if (!at.ok) { ok = false; return; }
          // At this stage, just enforce that *args expands to a list
          if (at.out != ast::TypeKind::List) { addDiag(*diags, "*args must be a list", sa.get()); ok = false; return; }
          // If vararg is annotated as list[T], ensure element compatibility when possible
          if (varargIdx != static_cast<size_t>(-1) && sig.full[varargIdx].listElemMask != 0U) {
            uint32_t elemMask = 0U;
            if (sa->kind == ast::NodeKind::ListLiteral) {
              const auto* lst = static_cast<const ast::ListLiteral*>(sa.get());
              for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
            } else if (sa->kind == ast::NodeKind::Name) {
              const auto* nm = static_cast<const ast::Name*>(sa.get()); elemMask = env->getListElems(nm->id);
            }
            if (elemMask != 0U && (elemMask & ~sig.full[varargIdx].listElemMask) != 0U) { addDiag(*diags, "*args element type mismatch", sa.get()); ok = false; return; }
          }
        }
      }
      // Positional
      for (size_t i = 0; i < callNode.args.size(); ++i) {
        ExpressionTyper at{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[i]->accept(at); if (!at.ok) { ok = false; return; }
        if (i < posParamIdxs.size()) {
          size_t pidx = posParamIdxs[i];
          if (at.out != sig.full[pidx].type) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
          bound[pidx] = true;
        } else if (varargIdx != static_cast<size_t>(-1)) {
          if (sig.full[varargIdx].type != ast::TypeKind::NoneType && at.out != sig.full[varargIdx].type) { addDiag(*diags, "*args element type mismatch", callNode.args[i].get()); ok = false; return; }
        } else { addDiag(*diags, std::string("arity mismatch calling function: ") + nameNode->id, &callNode); ok = false; return; }
      }
      // Keywords
      for (const auto& kw : callNode.keywords) {
        auto itn = nameToIdx.find(kw.name);
        if (itn == nameToIdx.end()) {
          if (kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, std::string("unknown keyword argument: ") + kw.name, &callNode); ok = false; return; }
          continue;
        }
        size_t pidx = itn->second;
        if (sig.full[pidx].isPosOnly) { addDiag(*diags, std::string("positional-only argument passed as keyword: ") + kw.name, &callNode); ok = false; return; }
        if (bound[pidx]) { addDiag(*diags, std::string("multiple values for argument: ") + kw.name, &callNode); ok = false; return; }
        ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets}; if (kw.value) kw.value->accept(kt); if (!kt.ok) { ok = false; return; }
        // Enforce union or list element annotations when present
        const auto& p = sig.full[pidx]; bool typeOk = false; const uint32_t aMask = TypeEnv::maskForKind(kt.out);
        if (p.unionMask != 0U) { typeOk = ((aMask & p.unionMask) != 0U); }
        else if (p.type == ast::TypeKind::List && p.listElemMask != 0U && kt.out == ast::TypeKind::List) {
          uint32_t elemMask = 0U;
          if (kw.value && kw.value->kind == ast::NodeKind::ListLiteral) {
            const auto* lst = static_cast<const ast::ListLiteral*>(kw.value.get());
            for (const auto& el : lst->elements) { if (!el) continue; ExpressionTyper et{*env, *sigs, *retParamIdxs, *diags, polyTargets}; el->accept(et); if (!et.ok) { ok = false; return; } elemMask |= (et.outSet != 0U) ? et.outSet : TypeEnv::maskForKind(et.out); }
          } else if (kw.value && kw.value->kind == ast::NodeKind::Name) {
            const auto* nm = static_cast<const ast::Name*>(kw.value.get()); elemMask = env->getListElems(nm->id);
          }
          if (elemMask != 0U) { typeOk = ((elemMask & ~p.listElemMask) == 0U); } else { typeOk = true; }
        } else { typeOk = (kt.out == p.type); }
        if (!typeOk) { addDiag(*diags, std::string("keyword argument type mismatch: ") + kw.name, &callNode); ok = false; return; }
        bound[pidx] = true;
      }
      if (!callNode.starArgs.empty() && varargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "*args provided but callee has no varargs", &callNode); ok = false; return; }
      if (!callNode.kwStarArgs.empty() && kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, "**kwargs provided but callee has no kwvarargs", &callNode); ok = false; return; }
      for (size_t i = 0; i < sig.full.size(); ++i) {
        const auto& sp = sig.full[i]; if (sp.isVarArg || sp.isKwVarArg) continue; if (!bound[i] && !sp.hasDefault) {
          if (sp.isKwOnly) { addDiag(*diags, std::string("missing required keyword-only argument: ") + sp.name, &callNode); ok = false; return; }
          else { addDiag(*diags, std::string("missing required positional argument: ") + sp.name, &callNode); ok = false; return; }
        }
      }
      out = sig.ret; mutableCall.setType(out);
    } else {
      if (sig.params.size() != callNode.args.size()) { addDiag(*diags, std::string("arity mismatch calling function: ") + nameNode->id, &callNode); ok = false; return; }
      for (size_t i = 0; i < callNode.args.size(); ++i) {
        ExpressionTyper argTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[i]->accept(argTyper); if (!argTyper.ok) { ok = false; return; }
        if (argTyper.out != sig.params[i]) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
      }
      out = sig.ret; mutableCall.setType(out);
    }
    // Interprocedural canonical propagation for trivial forwarders: f(x, ...) -> x
    if (callNode.callee && callNode.callee->kind == ast::NodeKind::Name) {
      const auto* cname = static_cast<const ast::Name*>(callNode.callee.get());
      auto retIdxIt = retParamIdxs->find(cname->id);
      if (retIdxIt != retParamIdxs->end()) {
        const int idx = retIdxIt->second;
        if (idx >= 0 && static_cast<size_t>(idx) < callNode.args.size()) {
          const auto& arg = callNode.args[idx];
          if (arg) { const auto& can = arg->canonical(); if (can) { const_cast<ast::Call&>(callNode).setCanonicalKey(*can); } }
        }
      }
    }
    return;
  }

  // 2) Attribute call: resolve attribute target and allow polymorphic dispatch via attrs map
  if (callNode.callee && callNode.callee->kind == ast::NodeKind::Attribute) {
    const auto* at = static_cast<const ast::Attribute*>(callNode.callee.get());
    // class method call: ClassName.method(...)
    if (at->value && at->value->kind == ast::NodeKind::Name) {
      const ast::Name* nameNode = static_cast<const ast::Name*>(at->value.get());
      const std::string key = nameNode->id + std::string(".") + at->attr;
      auto it = sigs->find(key);
      auto& mutableCall = const_cast<ast::Call&>(callNode);
      if (it != sigs->end()) {
        const Sig& sig = it->second;
        if (!sig.full.empty()) {
          std::unordered_map<std::string, size_t> nameToIdx; std::vector<size_t> posIdxs; size_t varargIdx = static_cast<size_t>(-1), kwvarargIdx = static_cast<size_t>(-1);
          posIdxs.reserve(sig.full.size());
          for (size_t i = 0; i < sig.full.size(); ++i) {
            const auto& p = sig.full[i]; if (p.isVarArg) varargIdx = i; else if (p.isKwVarArg) kwvarargIdx = i; else { nameToIdx[p.name] = i; if (!p.isKwOnly) posIdxs.push_back(i); }
          }
          std::vector<bool> bound(sig.full.size(), false);
          for (size_t i = 0; i < callNode.args.size(); ++i) {
            ExpressionTyper at2{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; callNode.args[i]->accept(at2); if (!at2.ok) { ok = false; return; }
            if (i < posIdxs.size()) { size_t pidx = posIdxs[i]; if (at2.out != sig.full[pidx].type) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; } bound[pidx] = true; }
            else if (varargIdx != static_cast<size_t>(-1)) { if (sig.full[varargIdx].type != ast::TypeKind::NoneType && at2.out != sig.full[varargIdx].type) { addDiag(*diags, "*args element type mismatch", callNode.args[i].get()); ok = false; return; } }
            else { addDiag(*diags, std::string("arity mismatch calling function: ") + (nameNode->id + std::string(".__call__")), &callNode); ok = false; return; }
          }
          for (const auto& kw : callNode.keywords) {
            auto itn = nameToIdx.find(kw.name);
            if (itn == nameToIdx.end()) { if (kwvarargIdx == static_cast<size_t>(-1)) { addDiag(*diags, std::string("unknown keyword argument: ") + kw.name, &callNode); ok = false; return; } continue; }
            size_t pidx = itn->second; if (bound[pidx]) { addDiag(*diags, std::string("multiple values for argument: ") + kw.name, &callNode); ok = false; return; }
            ExpressionTyper kt{*env, *sigs, *retParamIdxs, *diags, polyTargets, nullptr, classes}; if (kw.value) kw.value->accept(kt); if (!kt.ok) { ok = false; return; }
            if (kt.out != sig.full[pidx].type) { addDiag(*diags, std::string("keyword argument type mismatch: ") + kw.name, &callNode); ok = false; return; }
            bound[pidx] = true;
          }
          out = sig.ret; mutableCall.setType(out); return;
        }
        // Simple signature (no full params)
        if (sig.params.size() != callNode.args.size()) { addDiag(*diags, std::string("arity mismatch calling function: ") + key, &callNode); ok = false; return; }
        for (size_t i = 0; i < callNode.args.size(); ++i) {
          ExpressionTyper argTyper{*env, *sigs, *retParamIdxs, *diags, polyTargets}; callNode.args[i]->accept(argTyper); if (!argTyper.ok) { ok = false; return; }
          if (argTyper.out != sig.params[i]) { addDiag(*diags, "call argument type mismatch", callNode.args[i].get()); ok = false; return; }
        }
        out = sig.ret; mutableCall.setType(out); return;
      }
      // Attribute polymorphism: module.attr = function alias
      if (polyTargets.attrs != nullptr) {
        std::string k = nameNode->id + std::string(".") + at->attr;
        auto ita = polyTargets.attrs->find(k);
        if (ita != polyTargets.attrs->end() && !ita->second.empty()) { out = ast::TypeKind::NoneType; const_cast<ast::Call&>(callNode).setType(out); return; }
      }
    }
  }

  addDiag(*diags, "unknown call target", &callNode); ok = false; return;
}

