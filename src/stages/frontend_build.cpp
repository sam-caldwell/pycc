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

namespace pycc::stages {

auto Frontend::Build(const std::string& src, std::unique_ptr<ast::Node>& out_root, std::string& err) -> bool {
  metrics::Metrics::ScopedTimer timer(metrics::Metrics::Phase::Parse);
  if (!frontend::BuildMinimalReturnIntModule(src, out_root, err)) {
    return false;
  }
  ast::ASTGeometry geometry{};
  ast::ComputeGeometry(*out_root, geometry);
  metrics::Metrics::SetASTGeometry(geometry);
  return true;
}

}  // namespace pycc::stages
