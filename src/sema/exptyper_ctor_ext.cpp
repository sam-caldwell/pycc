/***
 * Name: ExpressionTyper::ExpressionTyper (extended)
 * Purpose: Initialize expression typer with class map context.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

ExpressionTyper::ExpressionTyper(const TypeEnv& env_, const std::unordered_map<std::string, Sig>& sigs_,
                                 const std::unordered_map<std::string, int>& retParamIdxs_, std::vector<Diagnostic>& diags_,
                                 PolyPtrs polyIn, const std::vector<const TypeEnv*>* outerScopes_,
                                 const std::unordered_map<std::string, ClassInfo>* classes_)
  : env(&env_), sigs(&sigs_), retParamIdxs(&retParamIdxs_), diags(&diags_), polyTargets(polyIn), outers(outerScopes_), classes(classes_) {}

