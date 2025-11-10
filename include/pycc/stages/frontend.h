/***
 * Name: pycc::stages::Frontend
 * Purpose: Stage class for parsing and initial AST construction.
 * Inputs: Source string
 * Outputs: AST root and metrics (geometry)
 * Theory of Operation: Uses frontend builder for MVP and computes geometry.
 */
#pragma once

#include <memory>
#include <string>

#include "pycc/ast/ast.h"
#include "pycc/metrics/metrics.h"

namespace pycc {
namespace stages {

class Frontend : public metrics::Metrics {
 public:
  /*** Build: Construct an AST for the source. */
  bool Build(const std::string& src, std::unique_ptr<ast::Node>& out_root, std::string& err);
};

}  // namespace stages
}  // namespace pycc

