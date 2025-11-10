/***
 * Name: pycc::stages::Backend
 * Purpose: Stage class for driving system clang to produce artifacts.
 * Inputs: LLVM IR path, output paths, build kind
 * Outputs: Assembly/object/binary depending on mode
 * Theory of Operation: Wraps backend::ClangFromIR with metrics timing.
 */
#pragma once

#include <string>

#include "pycc/backend/clang_build.h"
#include "pycc/metrics/metrics.h"

namespace pycc {
namespace stages {

class Backend : public metrics::Metrics {
 public:
  /*** EmitAsmSide: Emit assembly side artifact (clang -S). */
  bool EmitAsmSide(const std::string& ir_path, const std::string& asm_out, std::string& err);
  /*** Build: Build primary output (object or binary). */
  bool Build(const std::string& ir_path, const std::string& out, backend::BuildKind kind, std::string& err);
};

}  // namespace stages
}  // namespace pycc

