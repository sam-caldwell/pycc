/***
 * Name: pycc::frontend::BuildAST
 * Purpose: Build a minimal AST for a module with `def main() -> int: return <int>`.
 * Inputs: Source text
 * Outputs: AST root (Module) and error string on failure
 * Theory of Operation: Parses a simple `return` integer and constructs nodes.
 */
#pragma once

#include <memory>
#include <string>

#include "pycc/ast/ast.h"

namespace pycc::frontend {

    /*** BuildMinimalReturnIntModule: Build AST for minimal return-int program. */
    bool BuildMinimalReturnIntModule(const std::string& source,
                                     std::unique_ptr<ast::Node>& out_root,
                                     std::string& err);

}
