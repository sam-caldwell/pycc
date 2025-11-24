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
#include "sema/detail/checks/BuildSigs.h"
#include "sema/detail/checks/CollectClasses.h"
#include "sema/detail/checks/MergeClassBases.h"
#include "ast/AssignStmt.h"
#include "ast/ExprStmt.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/IntLiteral.h"
#include "ast/Module.h"
#include "ast/Name.h"
#include "ast/ReturnStmt.h"
#include <functional>
#include <unordered_set>

namespace pycc::sema {
    // Body moved verbatim from previous Sema.cpp
    bool sema_check_impl(pycc::sema::Sema *self, ast::Module &mod, std::vector<pycc::sema::Diagnostic> &diags) {
        auto &funcFlags_ = self->funcFlags_;
        auto &stmtMayRaise_ = self->stmtMayRaise_;
        std::unordered_map<std::string, Sig> sigs;
        detail::buildSigs(mod, sigs);
        std::unordered_map<std::string, ClassInfo> classes;
        detail::collectClasses(mod, classes, diags);
        detail::mergeClassBases(classes);
        // ReSharper disable once CppUseStructuredBinding
        for (const auto &kv: classes) { for (const auto &mkv: kv.second.methods) { sigs[kv.first + std::string(".") + mkv.first] = mkv.second; } }
        // Function trait scan for generator/coroutine flags
        scanFunctionTraits(mod, funcFlags_);
        std::unordered_map<std::string, int> retParamIdxs = computeReturnParamIdxs(mod);
        for (const auto &func: mod.functions) {
            TypeEnv env;
            Provenance prov{"", 0, 0};
            env.define("int", ast::TypeKind::Int, prov);
            env.define("float", ast::TypeKind::Float, prov);
            env.define("bool", ast::TypeKind::Bool, prov);
            env.define("str", ast::TypeKind::Str, prov);
            for (const auto &p: func->params) {
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
            for (const auto &dec: func->decorators) {
                if (!dec) continue;
                ast::TypeKind tmp{};
                std::vector<Diagnostic> scratch;
                (void) inferExprType(dec.get(), env, sigs, retParamIdxs, tmp, scratch, {});
            }
            // Minimal statement walker to force expression typing across the body
            struct SimpleStmtChecker : public ast::VisitorBase {
                const std::unordered_map<std::string, Sig> &sigs;
                const std::unordered_map<std::string, int> &retParamIdxs;
                const std::unordered_map<std::string, ClassInfo> *classes;
                TypeEnv &env;
                std::vector<Diagnostic> &diags;
                ast::TypeKind expectedReturn{ast::TypeKind::NoneType};
                bool ok{true};

                SimpleStmtChecker(const std::unordered_map<std::string, Sig> &s,
                                  const std::unordered_map<std::string, int> &r,
                                  const std::unordered_map<std::string, ClassInfo> *c,
                                  TypeEnv &e,
                                  std::vector<Diagnostic> &d)
                    : sigs(s), retParamIdxs(r), classes(c), env(e), diags(d) {
                }

                void visit(const ast::Module &) override {
                    //noop
                }

                void visit(const ast::FunctionDef &) override {
                    //noop
                }

                void visit(const ast::ExprStmt &es) override {
                    if (!ok) return;
                    if (es.value) {
                        ast::TypeKind t{};
                        ok &= inferExprType(es.value.get(), env, sigs, retParamIdxs, t, diags, {}, nullptr, classes);
                    }
                }

                void visit(const ast::AssignStmt &as) override {
                    if (!ok) return;
                    if (as.value) {
                        ast::TypeKind t{};
                        ok &= inferExprType(as.value.get(), env, sigs, retParamIdxs, t, diags, {}, nullptr, classes);
                        if (!ok) return;
                        // Best-effort: bind simple name targets in the environment
                        Provenance prov{"", 0, 0};
                        if (!as.targets.empty()) {
                            if (as.targets.size() == 1 && as.targets[0] && as.targets[0]->kind == ast::NodeKind::Name) {
                                const auto *nm = static_cast<const ast::Name *>(as.targets[0].get());
                                env.unionSet(nm->id, TypeEnv::maskForKind(t), prov);
                            }
                        } else if (!as.target.empty()) {
                            env.unionSet(as.target, TypeEnv::maskForKind(t), prov);
                        }
                    }
                }

                void visit(const ast::ReturnStmt &rs) override {
                    if (!ok) return;
                    if (rs.value) {
                        ast::TypeKind t{};
                        ok &= inferExprType(rs.value.get(), env, sigs, retParamIdxs, t, diags, {}, nullptr, classes);
                        if (!ok) return;
                        if (expectedReturn != ast::TypeKind::NoneType && t != expectedReturn) {
                            addDiag(diags, "return type mismatch", &rs);
                            ok = false;
                            return;
                        }
                    }
                }

                void visit(const ast::IfStmt &is) override {
                    if (!ok) return;
                    if (is.cond) {
                        ast::TypeKind t{};
                        ok &= inferExprType(is.cond.get(), env, sigs, retParamIdxs, t, diags, {}, nullptr, classes);
                        if (!ok) return;
                    }
                    // Branch under copies, then intersect resulting envs to refine deep merges
                    TypeEnv envThen = env;
                    TypeEnv envElse = env;
                    // Recurse into then
                    {
                        SimpleStmtChecker thenC{sigs, retParamIdxs, classes, envThen, diags};
                        thenC.expectedReturn = expectedReturn; // propagate expected return type
                        for (const auto &s: is.thenBody) { if (!s) continue; s->accept(thenC); if (!thenC.ok) { ok = false; return; } }
                    }
                    // Recurse into else
                    {
                        SimpleStmtChecker elseC{sigs, retParamIdxs, classes, envElse, diags};
                        elseC.expectedReturn = expectedReturn;
                        for (const auto &s: is.elseBody) { if (!s) continue; s->accept(elseC); if (!elseC.ok) { ok = false; return; } }
                    }
                    // Intersect and write back into current env (names present in both branches)
                    TypeEnv merged;
                    merged.intersectFrom(envThen, envElse);
                    env.applyMerged(merged);
                }

                // minimal stubs
                void visit(const ast::Literal<long long, ast::NodeKind::IntLiteral> &) override {
                    //noop
                }

                // minimal stubs
                void visit(const ast::Literal<double, ast::NodeKind::FloatLiteral> &) override {
                    //noop
                }

                void visit(const ast::Literal<bool, ast::NodeKind::BoolLiteral> &) override {
                    //noop
                }

                void visit(const ast::Literal<std::string, ast::NodeKind::StringLiteral> &) override {
                    //noop
                }

                void visit(const ast::Name &) override {
                    //noop
                }

                void visit(const ast::Call &) override {
                    //noop
                }

                void visit(const ast::Binary &) override {
                    //noop
                }

                void visit(const ast::Unary &) override {
                    //noop
                }

                void visit(const ast::TupleLiteral &) override {
                    //noop
                }

                void visit(const ast::ListLiteral &) override {
                    //noop
                }

                void visit(const ast::ObjectLiteral &) override {
                    //noop
                }

                void visit(const ast::NoneLiteral &) override {
                    //noop
                }
            };
            SimpleStmtChecker checker{sigs, retParamIdxs, &classes, env, diags};
            checker.expectedReturn = func->returnType;
            for (const auto &stmt: func->body) {
                if (!stmt) continue;
                stmt->accept(checker);
                if (!checker.ok) return false;
            }
        }
        scanStmtEffects(mod, stmtMayRaise_);
        return diags.empty();
    }
} // namespace pycc::sema
