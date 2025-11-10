//(c) 2025 Sam Caldwell.  See LICENSE.txt
//src/clang-tidy/docstrings-checker/Check.cpp
/***
 * Name: DocstringsChecker::check
 * Purpose: Validate docstrings for declarations/definitions and file headers.
 * Inputs: MatchResult
 * Outputs: Diagnostics on missing/invalid docstrings or file header lines
 * Theory of Operation: Use RawComment for docstrings and SourceManager for file header.
 */
#include "clang-tidy/docstrings-checker/DocstringsChecker.h"

#include <clang/AST/ASTContext.h>
#include <clang/AST/RawCommentList.h>
#include <clang/Basic/SourceManager.h>

namespace pycc::tidy {

static bool HasKeys(llvm::StringRef S) {
  return S.contains("Name:") && S.contains("Purpose:") && S.contains("Inputs:") &&
         S.contains("Outputs:") && S.contains("Theory of Operation:");
}

bool DocstringsChecker::HasRequiredDoc(const clang::Decl* D, const clang::ASTContext& Ctx) {
  if (!D) return false;
  if (auto* RC = Ctx.getRawCommentForDeclNoCache(D)) {
    llvm::StringRef T = RC->getRawText(Ctx.getSourceManager());
    if (T.trim().startswith("/***") && HasKeys(T)) return true;
  }
  return false;
}

bool DocstringsChecker::HasRequiredFileHeader(const clang::SourceManager& SM, clang::FileID FID) {
  if (FID.isInvalid()) return false;
  bool Invalid = false;
  llvm::StringRef B = SM.getBufferData(FID, &Invalid);
  if (Invalid) return false;
  // Check first two lines
  auto NL = B.find('\n');
  if (NL == llvm::StringRef::npos) return false;
  auto First = B.substr(0, NL).trim();
  auto Rest = B.substr(NL + 1);
  auto NL2 = Rest.find('\n');
  if (NL2 == llvm::StringRef::npos) return false;
  auto Second = Rest.substr(0, NL2).trim();
  if (!First.startswith("//(c) ")) return false;
  if (!Second.startswith("//")) return false;
  return true;
}

void DocstringsChecker::check(const clang::ast_matchers::MatchFinder::MatchResult& Result) {
  const auto* SM = Result.SourceManager;
  const auto& Ctx = *Result.Context;
  if (const auto* D = Result.Nodes.getNodeAs<clang::Decl>("decl")) {
    // File header check
    clang::FileID FID = SM->getFileID(D->getLocation());
    if (!HasRequiredFileHeader(*SM, FID)) {
      diag(D->getLocation(), "missing or invalid file header (copyright and filename)");
    }
    // Docstring check
    if (!HasRequiredDoc(D, Ctx)) {
      diag(D->getLocation(), "missing or invalid structured docstring (/*** ... */)");
    }
  }
}

}  // namespace pycc::tidy

