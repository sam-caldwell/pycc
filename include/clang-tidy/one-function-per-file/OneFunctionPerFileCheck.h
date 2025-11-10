//(c) 2025 Sam Caldwell.  See LICENSE.txt
//include/clang-tidy/one-function-per-file/OneFunctionPerFileCheck.h
/***
 * Name: pycc::tidy::OneFunctionPerFileCheck
 * Purpose: Enforce that each .cpp file defines at most one function or method.
 * Inputs: Clang-Tidy AST for a translation unit
 * Outputs: Diagnostics when multiple definitions are present in one file
 * Theory of Operation: Matches function/method definitions and counts per file;
 *          second and subsequent definitions trigger diagnostics.
 */
#pragma once

#include <unordered_map>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Tidy/ClangTidyCheck.h>

namespace pycc::tidy {

class OneFunctionPerFileCheck : public clang::tidy::ClangTidyCheck {
 public:
  explicit OneFunctionPerFileCheck(llvm::StringRef Name, clang::tidy::ClangTidyContext* Ctx)
      : ClangTidyCheck(Name, Ctx) {}

  /*** registerMatchers: Register matchers to capture function/method definitions. */
  void registerMatchers(clang::ast_matchers::MatchFinder* Finder) override;

  /*** check: Count per file and diagnose if more than one definition is detected. */
  void check(const clang::ast_matchers::MatchFinder::MatchResult& Result) override;

 private:
  std::unordered_map<const clang::FileEntry*, unsigned> DefCount_{};
};

}  // namespace pycc::tidy

