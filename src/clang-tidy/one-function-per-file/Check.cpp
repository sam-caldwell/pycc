//(c) 2025 Sam Caldwell.  See LICENSE.txt
//src/clang-tidy/one-function-per-file/Check.cpp
/***
 * Name: OneFunctionPerFileCheck::check
 * Purpose: Ensure only one function/method definition occurs per .cpp file.
 * Inputs: MatchResult with a matched definition
 * Outputs: Diagnostic on second and subsequent definitions in a file
 * Theory of Operation: Track counts keyed by FileEntry*.
 */
#include "clang-tidy/one-function-per-file/OneFunctionPerFileCheck.h"

#include <clang/Basic/SourceManager.h>

namespace pycc::tidy {

void OneFunctionPerFileCheck::check(const clang::ast_matchers::MatchFinder::MatchResult& Result) {
  const auto* SM = Result.SourceManager;
  if (const auto* ND = Result.Nodes.getNodeAs<clang::NamedDecl>("def")) {
    auto Loc = ND->getLocation();
    if (!Loc.isValid()) return;
    clang::FileID FID = SM->getFileID(Loc);
    const clang::FileEntry* FE = SM->getFileEntryForID(FID);
    if (!FE) return;
    if (!FE->getName().endswith(".cpp")) return;  // Only enforce for .cpp
    auto& C = DefCount_[FE];
    C += 1;
    if (C > 1) {
      diag(Loc, "only one function/method definition is allowed per .cpp file");
    }
  }
}

}  // namespace pycc::tidy

