//(c) 2025 Sam Caldwell.  See LICENSE.txt
//src/clang-tidy/module/RegisterModule.cpp
/***
 * Name: pycc::tidy module registration
 * Purpose: Register custom clang-tidy checks provided by pycc.
 * Inputs: Clang-Tidy framework
 * Outputs: Plugin module exposing our checks
 * Theory of Operation: Uses ClangTidyModuleFactory to expose checks by name.
 */
#include "clang-tidy/declare-only/DeclareOnlyCheck.h"
#include "clang-tidy/one-function-per-file/OneFunctionPerFileCheck.h"
#include "clang-tidy/docstrings-checker/DocstringsChecker.h"

#include <clang/Tidy/ClangTidy.h>

using namespace clang::tidy;

namespace {
class PyccTidyModule : public ClangTidyModule {
 public:
  void addCheckFactories(ClangTidyCheckFactories& CheckFactories) override {
    CheckFactories.registerCheck<pycc::tidy::DeclareOnlyCheck>("pycc-declare-only");
    CheckFactories.registerCheck<pycc::tidy::OneFunctionPerFileCheck>(
        "pycc-one-function-per-file");
    CheckFactories.registerCheck<pycc::tidy::DocstringsChecker>("pycc-docstrings");
  }
};
}  // namespace

// Register the module using this statically initialized variable.
static ClangTidyModuleRegistry::Add<PyccTidyModule> X("pycc-module",
                                                     "Pycc custom checks");

// This anchor is used to force the linker to link in the generated object file
// and thus register the module.
volatile int PyccTidyModuleAnchorSource = 0;

