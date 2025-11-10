//(c) 2025 Sam Caldwell.  See LICENSE.txt
//src/clang-tidy/declare-only/Check.cpp
/***
 * Name: DeclareOnlyCheck::check
 * Purpose: Enforce header constraints and one declaration per header file.
 * Inputs: MatchResult
 * Outputs: Diagnostics for violations
 * Theory of Operation: Use SourceManager to test files and counts.
 */
#include "clang-tidy/declare-only/DeclareOnlyCheck.h"

#include <clang/Basic/SourceManager.h>

namespace pycc::tidy {

void DeclareOnlyCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& Result) {
  const auto* SM = Result.SourceManager;
  const auto& Ctx = *Result.Context;

  // Header policy enforcement: function/method definitions should not be in headers.
  if (const auto* FD = Result.Nodes.getNodeAs<clang::FunctionDecl>("def")) {
    auto Loc = FD->getLocation();
    if (Loc.isValid()) {
      clang::FileID FID = SM->getFileID(Loc);
      const clang::FileEntry* FE = SM->getFileEntryForID(FID);
      if (FE && FE->getName().endswith(".h")) {
        diag(Loc, "function/method definition not allowed in header: %0") << FD->getNameInfo().getName();
      }
    }
  }

  // Count top-level declarations in headers: only one allowed.
  if (const auto* ND = Result.Nodes.getNodeAs<clang::NamedDecl>("decl")) {
    auto Loc = ND->getLocation();
    if (Loc.isValid()) {
      clang::FileID FID = SM->getFileID(Loc);
      const clang::FileEntry* FE = SM->getFileEntryForID(FID);
      if (FE && FE->getName().endswith(".h")) {
        auto& C = DeclCount_[FE];
        C += 1;
        if (C > 1) {
          diag(Loc, "only one top-level declaration is allowed per header file");
        }
      }
    }
  }
  (void)Ctx;
}

}  // namespace pycc::tidy

