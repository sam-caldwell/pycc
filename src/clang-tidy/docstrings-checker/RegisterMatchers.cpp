//(c) 2025 Sam Caldwell.  See LICENSE.txt
//src/clang-tidy/docstrings-checker/RegisterMatchers.cpp
/***
 * Name: DocstringsChecker::registerMatchers
 * Purpose: Match declarations and definitions requiring docstrings.
 * Inputs: MatchFinder
 * Outputs: Registered matchers
 * Theory of Operation: Match classes, enums, and functions/methods.
 */
#include "clang-tidy/docstrings-checker/DocstringsChecker.h"

using namespace clang::ast_matchers;

namespace pycc::tidy {

void DocstringsChecker::registerMatchers(MatchFinder* Finder) {
  if (!getLangOpts().CPlusPlus) return;
  Finder->addMatcher(decl(anyOf(cxxRecordDecl().bind("rec"),
                                enumDecl().bind("enm"),
                                functionDecl().bind("fun"),
                                cxxMethodDecl().bind("met")))
                         .bind("decl"),
                     this);
}

}  // namespace pycc::tidy

