/***
 * Name: pycc::frontend::BuildMinimalReturnIntModule
 * Purpose: Construct AST for `def main() -> int: return <int>` programs.
 * Inputs:
 *   - source: module text
 * Outputs:
 *   - out_root: AST Module root on success
 *   - err: error string on failure
 * Theory of Operation: Reuses the MVP return-int parsing to locate the integer and
 *   constructs Module -> FunctionDef -> ReturnStmt -> IntLiteral(value).
 */
// NOLINTNEXTLINE(misc-include-cleaner) - interface for BuildMinimalReturnIntModule
#include "pycc/frontend/build_ast.h"
// NOLINTNEXTLINE(misc-include-cleaner) - interface for ParseReturnInt
#include "pycc/frontend/simple_return_int.h"

#include "pycc/ast/ast.h"

#include <memory>
#include <string>
#include <utility>

namespace pycc {
namespace frontend {

bool BuildMinimalReturnIntModule(const std::string& source,
                                 std::unique_ptr<ast::Node>& out_root,
                                 std::string& err) {
  int value{};
  if (!ParseReturnInt(source, value, err)) {
    return false;
  }

  auto mod = std::make_unique<ast::Module>();
  auto func = std::make_unique<ast::FunctionDef>();
  auto ret = std::make_unique<ast::ReturnStmt>();
  auto lit = std::make_unique<ast::IntLiteral>(value);

  ret->children.emplace_back(std::move(lit));
  func->children.emplace_back(std::move(ret));
  mod->children.emplace_back(std::move(func));

  out_root = std::move(mod);
  return true;
}

}  // namespace frontend
}  // namespace pycc
