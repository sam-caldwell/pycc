/***
 * Name: pycc::stages::IREmitter
 * Purpose: Stage class for lowering AST to LLVM IR.
 * Inputs: AST root, module name
 * Outputs: LLVM IR string
 * Theory of Operation: Extracts the constant return value from the MVP AST and
 *   emits a minimal LLVM main returning that value.
 */
#pragma once

#include <string>

#include "pycc/ast/ast.h"
#include "pycc/metrics/metrics.h"

namespace pycc {
namespace stages {

class IREmitter : public metrics::Metrics {
 public:
  /*** Emit: Emit LLVM IR for the given AST. */
  bool Emit(const ast::Node& root, const std::string& module, std::string& out_ir, const std::string& src_hint);
};

}  // namespace stages
}  // namespace pycc

