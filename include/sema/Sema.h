/***
 * Name: pycc::sema::Sema
 * Purpose: Perform minimal type checking and name/arity validation before codegen.
 * Inputs:
 *   - AST Module
 * Outputs:
 *   - true/false with diagnostics messages
 * Theory of Operation:
 *   Gathers function signatures, then checks each function body:
 *   - Names must be defined before use
 *   - Binary ops require int operands and yield int
 *   - Calls must resolve to known functions with matching arity and int args
 *   - Return expressions must match declared return type
 */
#pragma once

#include "ast/Nodes.h"
#include "sema/TypeEnv.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace pycc::sema {

struct Diagnostic {
  std::string message;
  std::string file;
  int line{0};
  int col{0};
};

class Sema {
 public:
  // Annotates expression nodes with inferred TypeKind.
  bool check(ast::Module& mod, std::vector<Diagnostic>& diags);
};

} // namespace pycc::sema
