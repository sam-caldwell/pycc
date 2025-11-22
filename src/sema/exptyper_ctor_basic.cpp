/***
 * Name: ExpressionTyper::ExpressionTyper (basic)
 * Purpose: Initialize expression typer without class map context.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

ExpressionTyper::ExpressionTyper(const TypeEnv& env_, const std::unordered_map<std::string, Sig>& sigs_,
                                 const std::unordered_map<std::string, int>& retParamIdxs_, std::vector<Diagnostic>& diags_,
                                 PolyPtrs polyIn, const std::vector<const TypeEnv*>* outerScopes_)
  : env(&env_), sigs(&sigs_), retParamIdxs(&retParamIdxs_), diags(&diags_), polyTargets(polyIn), outers(outerScopes_), classes(nullptr) {}

