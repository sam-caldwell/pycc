/***
 * Name: pycc::stages::Backend::Build
 * Purpose: Build primary artifact (object or binary) with metrics timing.
 * Inputs:
 *   - ir_path: LLVM IR input
 *   - out: output file path
 *   - kind: ObjectOnly or Link
 * Outputs:
 *   - err: error on failure
 * Theory of Operation: Maps kind to Compile or Link phase and invokes clang.
 */
#include "pycc/stages/backend.h"

namespace pycc {
namespace stages {

bool Backend::Build(const std::string& ir_path, const std::string& out, backend::BuildKind kind, std::string& err) {
  metrics::Metrics::ScopedTimer t(kind == backend::BuildKind::Link
                                      ? metrics::Metrics::Phase::Link
                                      : metrics::Metrics::Phase::Compile);
  return backend::ClangFromIR(ir_path, out, kind, err);
}

}  // namespace stages
}  // namespace pycc

