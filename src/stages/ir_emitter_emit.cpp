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

#include <string>

#include "pycc/ir/emit_llvm_main_return.h"
#include "pycc/support/parse.h"

namespace pycc::stages {

auto IREmitter::Emit(const ast::Node& root, const std::string& module, std::string& out_ir, const std::string& src_hint)
    -> bool {
  metrics::Metrics::ScopedTimer timer(metrics::Metrics::Phase::EmitIR);

  int return_value = 0;
  bool found_literal = false;
  for (const auto& function_node : root.children) {
    if (function_node->kind != ast::NodeKind::FunctionDef) {
      continue;
    }
    for (const auto& stmt_node : function_node->children) {
      if (stmt_node->kind != ast::NodeKind::ReturnStmt) {
        continue;
      }
      if (!stmt_node->children.empty() && stmt_node->children[0]->kind == ast::NodeKind::IntLiteral) {
        const auto* literal = dynamic_cast<const ast::IntLiteral*>(stmt_node->children[0].get());
        if (literal != nullptr) {
          return_value = literal->payload;
          found_literal = true;
          break;
        }
      }
    }
    if (found_literal) {
      break;
    }
  }
  if (!found_literal) {
    constexpr std::string_view kReturn = "return ";
    const std::size_t pos = src_hint.find(kReturn);
    if (pos != std::string::npos) {
      const std::string_view view{src_hint};
      int temp_value = 0;
      if (support::ParseIntLiteralStrict(view.substr(pos + kReturn.size()), temp_value, nullptr)) {
        return_value = temp_value;
      }
    }
  }

  if (!ir::EmitLLVMMainReturnInt(return_value, module, out_ir)) {
    return false;
  }
  metrics::Metrics::RecordOptimization("LoweredConstantReturn(main)");
  return true;
}

}  // namespace pycc::stages
