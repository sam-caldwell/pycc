//(c) 2025 Sam Caldwell.  See LICENSE.txt
//include/clang-tidy/declare-only/DeclareOnlyCheck.h
/***
 * Name: pycc::tidy::DeclareOnlyCheck
 * Purpose: Enforce that headers contain only declarations, template-related
 *          definitions, or truly trivial one-line inline definitions; ensure
 *          only one top-level declaration (class/struct/enum/function) per file.
 * Inputs: Clang-Tidy AST for a translation unit
 * Outputs: Diagnostics when violations are detected
 * Theory of Operation: Matches header-situated definitions and counts top-level
 *          declarations per file; emits diagnostics accordingly.
 */
#pragma once

#include <string>
#include <unordered_map>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tidy/ClangTidyCheck.h>

namespace pycc::tidy {

class DeclareOnlyCheck : public clang::tidy::ClangTidyCheck {
 public:
  // One-line inline ctor allowed per repo rules.
  explicit DeclareOnlyCheck(llvm::StringRef Name, clang::tidy::ClangTidyContext* Ctx)
      : ClangTidyCheck(Name, Ctx) {}

  /***
   * Name: DeclareOnlyCheck::registerMatchers
   * Purpose: Register matchers to capture header-situated definitions and all
   *          top-level declarations.
   * Inputs: Finder, Compiler
   * Outputs: None (records matchers)
   * Theory of Operation: Use AST matchers to select function/class/enum
   *          declarations and definitions, then check in `check`.
   */
  void registerMatchers(clang::ast_matchers::MatchFinder* Finder) override;

  /***
   * Name: DeclareOnlyCheck::check
   * Purpose: Evaluate matched nodes and emit diagnostics when constraints are
   *          violated.
   * Inputs: Result (from matcher)
   * Outputs: Diagnostics
   * Theory of Operation: If a definition occurs in a header, or if a header has
   *          more than one top-level declaration, issue a diagnostic.
   */
  void check(const clang::ast_matchers::MatchFinder::MatchResult& Result) override;

 private:
  std::unordered_map<const clang::FileEntry*, unsigned> DeclCount_{};
};

}  // namespace pycc::tidy

