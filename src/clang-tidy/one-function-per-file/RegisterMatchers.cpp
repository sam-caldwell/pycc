//(c) 2025 Sam Caldwell.  See LICENSE.txt
//src/clang-tidy/one-function-per-file/RegisterMatchers.cpp
/***
 * Name: OneFunctionPerFileCheck::registerMatchers
 * Purpose: Match function and method definitions in .cpp files.
 * Inputs: MatchFinder
 * Outputs: Registered matchers
 * Theory of Operation: The `check` method will count and report per file.
 */
#include "clang-tidy/one-function-per-file/OneFunctionPerFileCheck.h"

using namespace clang::ast_matchers;

namespace pycc::tidy {

void OneFunctionPerFileCheck::registerMatchers(MatchFinder* Finder) {
  if (!getLangOpts().CPlusPlus) return;
  Finder->addMatcher(functionDecl(isDefinition()).bind("def"), this);
  Finder->addMatcher(cxxMethodDecl(isDefinition()).bind("def"), this);
}

}  // namespace pycc::tidy

