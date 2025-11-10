/***
 * Name: pycc::stages::IREmitter::Emit
 * Purpose: Lower AST to LLVM IR and record EmitIR timing and optimization.
 * Inputs:
 *   - root: AST root
 *   - module: module identifier for IR
 *   - src_hint: original source (used as fallback to find constant)
 * Outputs:
 *   - out_ir: resulting LLVM IR string
 * Theory of Operation: In MVP, searches for IntLiteral under ReturnStmt; otherwise
 *   falls back to parsing `return <int>` from src_hint. Emits using IR helper.
 */
#include "pycc/stages/ir_emitter.h"

#include <cstddef>
#include <optional>
#include <string>

#include "pycc/ast/ast.h"              // for direct use of ast::Node, ast::IntLiteral
#include "pycc/ir/emit_llvm_main_return.h"
#include "pycc/metrics/metrics.h"       // for direct use of Metrics::ScopedTimer
#include "pycc/stages/detail/ir_helpers.h"

namespace pycc::stages {


auto IREmitter::Emit(const ast::Node& root, const std::string& module, std::string& out_ir,
                     const std::string& src_hint) -> bool {
  const metrics::Metrics::ScopedTimer timer(metrics::Metrics::Phase::EmitIR);

  int return_value = 0;
  if (const auto val_opt = detail::FindReturnIntLiteral(root); val_opt.has_value()) {
    return_value = *val_opt;
  } else if (const auto parsed = detail::ParseReturnIntFromSource(src_hint); parsed.has_value()) {
    return_value = *parsed;
  }

  if (!ir::EmitLLVMMainReturnInt(return_value, module, out_ir)) {
    return false;
  }
  metrics::Metrics::RecordOptimization("LoweredConstantReturn(main)");
  return true;
}

}  // namespace pycc::stages
