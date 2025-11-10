//(c) 2025 Sam Caldwell.  See LICENSE.txt
//src/clang-tidy/declare-only/RegisterMatchers.cpp
/***
 * Name: DeclareOnlyCheck::registerMatchers
 * Purpose: Register matchers for header-situated definitions and top-level decls.
 * Inputs: MatchFinder, Compiler
 * Outputs: Registered matchers
 * Theory of Operation: Let `check` enforce constraints per matched node.
 */
#include "clang-tidy/declare-only/DeclareOnlyCheck.h"

using namespace clang::ast_matchers;

namespace pycc::tidy {

void DeclareOnlyCheck::registerMatchers(MatchFinder* Finder) {
  if (!getLangOpts().CPlusPlus) return;
  // Match any function or method definition.
  Finder->addMatcher(functionDecl(isDefinition()).bind("def"), this);
  Finder->addMatcher(cxxMethodDecl(isDefinition()).bind("def"), this);
  // Match top-level declarations: class/struct/enum/function
  Finder->addMatcher(namedDecl(anyOf(cxxRecordDecl(isDefinition()).bind("rec"),
                                     enumDecl().bind("enm"),
                                     functionDecl().bind("fun")))
                         .bind("decl"),
                     this);
}

}  // namespace pycc::tidy

