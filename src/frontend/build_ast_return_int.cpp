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
#include "pycc/frontend/build_ast.h"
#include "pycc/frontend/simple_return_int.h"

#include <memory>
#include <string>

namespace pycc {
namespace frontend {

bool BuildMinimalReturnIntModule(const std::string& source,
                                 std::unique_ptr<ast::Node>& out_root,
                                 std::string& err) {
  int value = 0;
  if (!ParseReturnInt(source, value, err)) return false;

  auto mod = std::make_unique<ast::Module>();
  auto fn = std::make_unique<ast::FunctionDef>();
  auto ret = std::make_unique<ast::ReturnStmt>();
  auto lit = std::make_unique<ast::IntLiteral>(value);

  ret->children.emplace_back(std::move(lit));
  fn->children.emplace_back(std::move(ret));
  mod->children.emplace_back(std::move(fn));

  out_root = std::move(mod);
  return true;
}

}  // namespace frontend
}  // namespace pycc

