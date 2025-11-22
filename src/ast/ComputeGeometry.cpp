/**
 * @file
 * @brief AST geometry computation implementation.
 */
/***
 * Name: pycc::ast::ComputeGeometry
 * Purpose: Traverse AST and compute node count and max depth.
 */
#include "ast/GeometrySummary.h"
#include "ast/GeometryVisitor.h"

namespace pycc::ast {

GeometrySummary ComputeGeometry(const Module& module) {
  GeometryVisitor visitor;
  module.accept(visitor);
  return GeometrySummary{visitor.nodes, visitor.maxDepth};
}

} // namespace pycc::ast
