/***
 * Name: sema_check_impl (definition)
 * Purpose: Internal implementation routine behind Sema::check, split into its
 *          own translation unit for modularity.
 */
#include "sema/detail/SemaImpl.h"
#include "sema/detail/Helpers.h"
#include "sema/detail/ExpressionTyper.h"
#include "sema/detail/LocalsAssigned.h"
#include "sema/detail/LocalsAssignedScan.h"
#include "sema/detail/ReturnParamScan.h"
#include "sema/detail/FnTraitScan.h"
#include "sema/detail/EffStmtScan.h"
#include "ast/AssignStmt.h"
#include "ast/Attribute.h"
#include "ast/Binary.h"
#include "ast/BoolLiteral.h"
#include "ast/Call.h"
#include "ast/ExprStmt.h"
#include "ast/FloatLiteral.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/IntLiteral.h"
#include "ast/ListLiteral.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/NoneLiteral.h"
#include "ast/ReturnStmt.h"
#include "ast/StringLiteral.h"
#include "ast/TupleLiteral.h"
#include "ast/Unary.h"
#include <functional>
#include <unordered_set>

namespace pycc::sema {

// Body moved verbatim from previous Sema.cpp
bool sema_check_impl(pycc::sema::Sema* self, ast::Module& mod, std::vector<pycc::sema::Diagnostic>& diags) {
  auto& funcFlags_ = self->funcFlags_;
  auto& stmtMayRaise_ = self->stmtMayRaise_;
  std::unordered_map<std::string, Sig> sigs;
  for (const auto& func : mod.functions) {
    Sig sig; sig.ret = func->returnType;
    for (const auto& param : func->params) {
      sig.params.push_back(param.type);
      SigParam sp; sp.name = param.name; sp.type = param.type; sp.isVarArg = param.isVarArg; sp.isKwVarArg = param.isKwVarArg; sp.isKwOnly = param.isKwOnly; sp.isPosOnly = param.isPosOnly; sp.hasDefault = (param.defaultValue != nullptr);
      if (!param.unionTypes.empty()) { uint32_t m = 0U; for (auto ut : param.unionTypes) { m |= TypeEnv::maskForKind(ut); } sp.unionMask = m; }
      if (param.type == ast::TypeKind::List && param.listElemType != ast::TypeKind::NoneType) { sp.listElemMask = TypeEnv::maskForKind(param.listElemType); }
      sig.full.push_back(std::move(sp));
    }
    sigs[func->name] = std::move(sig);
  }
  std::unordered_map<std::string, ClassInfo> classes;
  for (const auto& clsPtr : mod.classes) {
    if (!clsPtr) continue;
    ClassInfo ci;
    for (const auto& b : clsPtr->bases) { if (b && b->kind == ast::NodeKind::Name) { const auto* nm = static_cast<const ast::Name*>(b.get()); ci.bases.push_back(nm->id); } }
    for (const auto& st : clsPtr->body) {
      if (!st) continue;
      if (st->kind == ast::NodeKind::DefStmt) {
        const auto* ds = static_cast<const ast::DefStmt*>(st.get());
        if (ds->func) {
          const auto* fn = ds->func.get();
          if (fn->name == "__init__" && fn->returnType != ast::TypeKind::NoneType) { addDiag(diags, std::string("__init__ must return NoneType in class: ") + clsPtr->name, fn); }
          if (fn->name == "__len__" && fn->returnType != ast::TypeKind::Int) { addDiag(diags, std::string("__len__ must return int in class: ") + clsPtr->name, fn); }
          if (fn->name == "__get__") { const size_t n = fn->params.size(); if (!(n == 2 || n == 3)) { addDiag(diags, std::string("__get__ must take 2 or 3 params in class: ") + clsPtr->name, fn); } }
          if (fn->name == "__set__") { const size_t n = fn->params.size(); if (n != 3) { addDiag(diags, std::string("__set__ must take exactly 3 params in class: ") + clsPtr->name, fn); } }
          if (fn->name == "__delete__") { const size_t n = fn->params.size(); if (n != 2) { addDiag(diags, std::string("__delete__ must take exactly 2 params in class: ") + clsPtr->name, fn); } }
          if (fn->name == "__getattr__") { const size_t n = fn->params.size(); if (n != 2) { addDiag(diags, std::string("__getattr__ must take exactly 2 params in class: ") + clsPtr->name, fn); } }
          if (fn->name == "__getattribute__") { const size_t n = fn->params.size(); if (n != 2) { addDiag(diags, std::string("__getattribute__ must take exactly 2 params in class: ") + clsPtr->name, fn); } }
          if (fn->name == "__setattr__") { const size_t n = fn->params.size(); if (n != 3) { addDiag(diags, std::string("__setattr__ must take exactly 3 params in class: ") + clsPtr->name, fn); } }
          if (fn->name == "__delattr__") { const size_t n = fn->params.size(); if (n != 2) { addDiag(diags, std::string("__delattr__ must take exactly 2 params in class: ") + clsPtr->name, fn); } }
          if (fn->name == "__bool__" && fn->returnType != ast::TypeKind::Bool) { addDiag(diags, std::string("__bool__ must return bool in class: ") + clsPtr->name, fn); }
          if ((fn->name == "__str__" || fn->name == "__repr__") && fn->returnType != ast::TypeKind::Str) { addDiag(diags, std::string(fn->name) + std::string(" must return str in class: ") + clsPtr->name, fn); }
          Sig ms; ms.ret = fn->returnType;
          for (const auto& p : fn->params) { ms.params.push_back(p.type); SigParam sp; sp.name = p.name; sp.type = p.type; sp.isVarArg = p.isVarArg; sp.isKwVarArg = p.isKwVarArg; sp.isKwOnly = p.isKwOnly; sp.isPosOnly = p.isPosOnly; sp.hasDefault = (p.defaultValue != nullptr); ms.full.push_back(std::move(sp)); }
          ci.methods[fn->name] = std::move(ms);
        }
      }
    }
    classes[clsPtr->name] = std::move(ci);
  }
  std::function<void(const std::string&, ClassInfo&)> mergeFromBase;
  mergeFromBase = [&](const std::string& baseName, ClassInfo& dest) {
    auto itb = classes.find(baseName); if (itb == classes.end()) return;
    for (const auto& mkv : itb->second.methods) { if (dest.methods.find(mkv.first) == dest.methods.end()) { dest.methods[mkv.first] = mkv.second; } }
    for (const auto& bb : itb->second.bases) { mergeFromBase(bb, dest); }
  };
  for (const auto& clsPtr : mod.classes) { if (!clsPtr) continue; auto itc = classes.find(clsPtr->name); if (itc == classes.end()) continue; for (const auto& bn : itc->second.bases) { mergeFromBase(bn, itc->second); } }
  for (const auto& kv : classes) { for (const auto& mkv : kv.second.methods) { sigs[kv.first + std::string(".") + mkv.first] = mkv.second; } }
  // Function trait scan for generator/coroutine flags
  scanFunctionTraits(mod, funcFlags_);
  std::unordered_map<std::string, int> retParamIdxs = computeReturnParamIdxs(mod);
  for (const auto& func : mod.functions) {
    TypeEnv env;
    Provenance prov{"", 0, 0};
    env.define("int", ast::TypeKind::Int, prov);
    env.define("float", ast::TypeKind::Float, prov);
    env.define("bool", ast::TypeKind::Bool, prov);
    env.define("str", ast::TypeKind::Str, prov);
    for (const auto& p : func->params) {
      env.define(p.name, p.type, prov);
      if (p.type == ast::TypeKind::List && p.listElemType != ast::TypeKind::NoneType) {
        env.defineListElems(p.name, TypeEnv::maskForKind(p.listElemType));
      }
    }
    // Pre-scan locals-assigned for referenced-before-assignment checks
    std::unordered_set<std::string> locals;
    detail::scanLocalsAssigned(*func, locals);
    detail::ScopedLocalsAssigned guard(&locals);

    // Decorators: type-check expressions but ignore diagnostics for external names
    for (const auto& dec : func->decorators) { if (!dec) continue; ast::TypeKind tmp{}; std::vector<Diagnostic> scratch; (void)inferExprType(dec.get(), env, sigs, retParamIdxs, tmp, scratch, {}); }
    // Minimal statement walker to force expression typing across the body
    struct SimpleStmtChecker : public ast::VisitorBase {
      const std::unordered_map<std::string, Sig>& sigs;
      const std::unordered_map<std::string, int>& retParamIdxs;
      const std::unordered_map<std::string, ClassInfo>* classes;
      TypeEnv& env; std::vector<Diagnostic>& diags; bool ok{true};
      SimpleStmtChecker(const std::unordered_map<std::string, Sig>& s,
                        const std::unordered_map<std::string, int>& r,
                        const std::unordered_map<std::string, ClassInfo>* c,
                        TypeEnv& e,
                        std::vector<Diagnostic>& d)
        : sigs(s), retParamIdxs(r), classes(c), env(e), diags(d) {}
      void visit(const ast::Module&) override {}
      void visit(const ast::FunctionDef&) override {}
      void visit(const ast::ExprStmt& es) override { if (!ok) return; if (es.value) { ast::TypeKind t{}; ok &= inferExprType(es.value.get(), env, sigs, retParamIdxs, t, diags, {}, nullptr, classes); } }
      void visit(const ast::AssignStmt& as) override {
        if (!ok) return;
        if (as.value) {
          ast::TypeKind t{};
          ok &= inferExprType(as.value.get(), env, sigs, retParamIdxs, t, diags, {}, nullptr, classes);
          if (!ok) return;
          // Best-effort: bind simple name targets in the environment
          Provenance prov{"", 0, 0};
          if (!as.targets.empty()) {
            if (as.targets.size() == 1 && as.targets[0] && as.targets[0]->kind == ast::NodeKind::Name) {
              const auto* nm = static_cast<const ast::Name*>(as.targets[0].get());
              env.unionSet(nm->id, TypeEnv::maskForKind(t), prov);
            }
          } else if (!as.target.empty()) {
            env.unionSet(as.target, TypeEnv::maskForKind(t), prov);
          }
        }
      }
      void visit(const ast::ReturnStmt& rs) override { if (!ok) return; if (rs.value) { ast::TypeKind t{}; ok &= inferExprType(rs.value.get(), env, sigs, retParamIdxs, t, diags, {}, nullptr, classes); } }
      void visit(const ast::IfStmt& is) override {
        if (!ok) return; if (is.cond) { ast::TypeKind t{}; ok &= inferExprType(is.cond.get(), env, sigs, retParamIdxs, t, diags, {}, nullptr, classes); }
        for (const auto& s : is.thenBody) { if (!s) continue; s->accept(*this); if (!ok) return; }
        for (const auto& s : is.elseBody) { if (!s) continue; s->accept(*this); if (!ok) return; }
      }
      // minimal stubs
      void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral>&) override {}
      void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral>&) override {}
      void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral>&) override {}
      void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral>&) override {}
      void visit(const ast::Name&) override {}
      void visit(const ast::Call&) override {}
      void visit(const ast::Binary&) override {}
      void visit(const ast::Unary&) override {}
      void visit(const ast::TupleLiteral&) override {}
      void visit(const ast::ListLiteral&) override {}
      void visit(const ast::ObjectLiteral&) override {}
      void visit(const ast::NoneLiteral&) override {}
    };
    SimpleStmtChecker checker{sigs, retParamIdxs, &classes, env, diags};
    for (const auto& stmt : func->body) { if (!stmt) continue; stmt->accept(checker); if (!checker.ok) return false; }
  }
  scanStmtEffects(mod, stmtMayRaise_);
  return diags.empty();
}

} // namespace pycc::sema
