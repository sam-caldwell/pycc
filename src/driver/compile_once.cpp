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

#include <iostream>
#include <memory>
#include <string>

#include "pycc/ast/ast.h"
// driver includes
#include "pycc/driver/cli.h"  // direct use of driver::CliOptions
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

namespace pycc::driver {

auto CompileOnce(const driver::CliOptions& opts, const std::string& input_path) -> int {
  std::string source_text;
  std::string error_message;
  if (!stages::FileReader::Read(input_path, source_text, error_message)) {
    std::cerr << "pycc: " << error_message << '\n';
    return 2;
  }

  std::unique_ptr<ast::Node> root;
  if (!stages::Frontend::Build(source_text, root, error_message)) {
    std::cerr << "pycc: parse error: " << error_message << '\n';
    return 2;
  }

  std::string ir_text;
  if (!stages::IREmitter::Emit(*root, input_path, ir_text, source_text)) {
    std::cerr << "pycc: internal error: failed to emit IR" << '\n';
    return 2;
  }

  const auto outputs = DeriveOutputs(opts.output);
  if (PYCC_EMIT_LLVM && !WriteFileOrReport(outputs.ll, ir_text, error_message)) {
    return 2;
  }

  if (PYCC_EMIT_ASM && !opts.emit_asm) {
    if (!WriteFileOrReport(outputs.ll, ir_text, error_message)) {  // ensure IR exists on disk
      return 2;
    }
    if (!stages::Backend::EmitAsmSide(outputs.ll, outputs.s, error_message)) {
      std::cerr << "pycc: " << error_message << '\n';
      return 2;
    }
  }

  if (!WriteFileOrReport(outputs.ll, ir_text, error_message)) {  // ensure IR before primary build
    return 2;
  }
  const auto [kind, target] = SelectBuildTarget(opts, outputs);
  if (!stages::Backend::Build(outputs.ll, target, kind, error_message)) {
    std::cerr << "pycc: " << error_message << '\n';
    return 2;
  }
  return 0;
}

}  // namespace pycc::driver
