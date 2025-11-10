/***
 * Name: pycc::stages::Frontend::Build
 * Purpose: Build an AST for the source and record geometry and timing.
 * Inputs:
 *   - src: module text
 * Outputs:
 *   - out_root: AST root on success
 *   - err: error string on failure
 * Theory of Operation: Uses MVP builder and ComputeGeometry; stores metrics.
 */
#include "pycc/stages/frontend.h"

#include "pycc/frontend/build_ast.h"

namespace pycc {
namespace stages {

bool Frontend::Build(const std::string& src, std::unique_ptr<ast::Node>& out_root, std::string& err) {
  metrics::Metrics::ScopedTimer t(metrics::Metrics::Phase::Parse);
  if (!frontend::BuildMinimalReturnIntModule(src, out_root, err)) return false;
  ast::ASTGeometry g;
  ast::ComputeGeometry(*out_root, g);
  metrics::Metrics::SetASTGeometry(g);
  return true;
}

}  // namespace stages
}  // namespace pycc

