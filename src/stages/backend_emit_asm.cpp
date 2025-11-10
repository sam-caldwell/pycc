/***
 * Name: pycc::stages::Backend::EmitAsmSide
 * Purpose: Emit assembly side artifact and record EmitASM timing.
 * Inputs:
 *   - ir_path: LLVM IR file path
 *   - asm_out: assembly output path
 * Outputs:
 *   - err: error string on failure
 * Theory of Operation: Calls ClangFromIR with BuildKind::AssembleOnly.
 */
#include "pycc/stages/backend.h"

namespace pycc {
namespace stages {

bool Backend::EmitAsmSide(const std::string& ir_path, const std::string& asm_out, std::string& err) {
  metrics::Metrics::ScopedTimer t(metrics::Metrics::Phase::EmitASM);
  return backend::ClangFromIR(ir_path, asm_out, backend::BuildKind::AssembleOnly, err);
}

}  // namespace stages
}  // namespace pycc

