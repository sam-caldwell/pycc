//(c) 2025 Sam Caldwell.  See LICENSE.txt
//include/clang-tidy/docstrings-checker/DocstringsChecker.h
/***
 * Name: pycc::tidy::DocstringsChecker
 * Purpose: Enforce presence and content of structured docstrings preceding each
 *          declaration and definition; enforce file header copyright and filename.
 * Inputs: Clang AST + RawComment extraction
 * Outputs: Diagnostics when required docstrings or file headers are missing
 * Theory of Operation: For matched declarations/definitions, query RawComment
 *          and check for '/***' header with required keys; also check file start.
 */
#pragma once

#include <string>

#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Tidy/ClangTidyCheck.h>

namespace pycc::tidy {

class DocstringsChecker : public clang::tidy::ClangTidyCheck {
 public:
  explicit DocstringsChecker(llvm::StringRef Name, clang::tidy::ClangTidyContext* Ctx)
      : ClangTidyCheck(Name, Ctx) {}

  /*** registerMatchers: Match top-level decls and definitions we care about. */
  void registerMatchers(clang::ast_matchers::MatchFinder* Finder) override;

  /*** check: Validate docstring presence and basic content; validate file header. */
  void check(const clang::ast_matchers::MatchFinder::MatchResult& Result) override;

 private:
  static bool HasRequiredDoc(const clang::Decl* D, const clang::ASTContext& Ctx);
  static bool HasRequiredFileHeader(const clang::SourceManager& SM, clang::FileID FID);
};

}  // namespace pycc::tidy

