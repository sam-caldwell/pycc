/***
 * Name: pycc::driver::CompileOnce
 * Purpose: Execute one end-to-end compile (read → frontend → IR → emit).
 * Inputs:
 *   - opts: CLI options
 *   - input_path: Path to input Python source
 * Outputs:
 *   - int: 0 on success; 2 on error with message printed
 * Theory of Operation: Orchestrates stages and writes artifacts, honoring -S/-c.
 */
#include "pycc/driver/app.h"

#include <memory>
#include <string>
#include <iostream>

#include "pycc/ast/ast.h"
#include "pycc/stages/backend.h"
#include "pycc/stages/file_reader.h"
#include "pycc/stages/frontend.h"
#include "pycc/stages/ir_emitter.h"

#ifndef PYCC_EMIT_LLVM
#define PYCC_EMIT_LLVM 0
#endif
#ifndef PYCC_EMIT_ASM
#define PYCC_EMIT_ASM 0
#endif

namespace pycc {
namespace driver {

int CompileOnce(const driver::CliOptions& opts, const std::string& input_path) {
  std::string src, err;
  if (stages::FileReader reader; !reader.Read(input_path, src, err)) {
    std::cerr << "pycc: " << err << '\n';
    return 2;
  }

  std::unique_ptr<ast::Node> root;
  if (stages::Frontend front; !front.Build(src, root, err)) {
    std::cerr << "pycc: parse error: " << err << '\n';
    return 2;
  }

  std::string ir;
  if (stages::IREmitter ire; !ire.Emit(*root, input_path, ir, src)) {
    std::cerr << "pycc: internal error: failed to emit IR" << '\n';
    return 2;
  }

  const auto outs = DeriveOutputs(opts.output);
  if (PYCC_EMIT_LLVM && !WriteFileOrReport(outs.ll, ir, err)) return 2;

  stages::Backend be;
  if (PYCC_EMIT_ASM && !opts.emit_asm) {
    if (!WriteFileOrReport(outs.ll, ir, err)) return 2;  // ensure IR exists on disk
    if (!be.EmitAsmSide(outs.ll, outs.s, err)) {
      std::cerr << "pycc: " << err << '\n';
      return 2;
    }
  }

  if (!WriteFileOrReport(outs.ll, ir, err)) return 2;  // ensure IR before primary build
  const auto [kind, target] = SelectBuildTarget(opts, outs);
  if (!be.Build(outs.ll, target, kind, err)) {
    std::cerr << "pycc: " << err << '\n';
    return 2;
  }
  return 0;
}

}  // namespace driver
}  // namespace pycc
