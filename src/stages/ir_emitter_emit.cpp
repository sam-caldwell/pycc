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

namespace pycc {
namespace stages {

bool IREmitter::Emit(const ast::Node& root, const std::string& module, std::string& out_ir, const std::string& src_hint) {
  metrics::Metrics::ScopedTimer t(metrics::Metrics::Phase::EmitIR);

  // Try to extract return value from AST (Module->Function->Return->IntLiteral)
  int ret_val = 0;
  bool found = false;
  for (const auto& c1 : root.children) {
    if (c1->kind != ast::NodeKind::FunctionDef) continue;
    for (const auto& c2 : c1->children) {
      if (c2->kind != ast::NodeKind::ReturnStmt) continue;
      if (!c2->children.empty() && c2->children[0]->kind == ast::NodeKind::IntLiteral) {
        const auto* lit = static_cast<const ast::IntLiteral*>(c2->children[0].get());
        ret_val = lit->payload;  // NodeT payload
        found = true;
        break;
      }
    }
    if (found) break;
  }
  if (!found) {
    const std::string key = "return ";
    auto pos = src_hint.find(key);
    if (pos != std::string::npos) {
      int temp;
      if (support::ParseIntLiteralStrict(std::string_view(src_hint).substr(pos + key.size()), temp, nullptr)) {
        ret_val = temp;
      }
    }
  }

  if (!ir::EmitLLVMMainReturnInt(ret_val, module, out_ir)) return false;
  metrics::Metrics::RecordOptimization("LoweredConstantReturn(main)");
  return true;
}

}  // namespace stages
}  // namespace pycc
