/***
 * Name: pycc::codegen::Codegen
 * Purpose: Lower a minimal AST to LLVM IR text and optionally assemble/link.
 * Inputs:
 *   - AST (Module) and output base path
 *   - Flags for -S (assembly only) and -c (object only)
 * Outputs:
 *   - Writes .ll, optionally .asm or final binary/object via clang
 * Theory of Operation:
 *   For Milestone 1, generate IR for functions that return integer literals.
 *   Use external clang to produce assembly/object/binary to avoid libLLVM.
 */
#pragma once

#include "ast/Nodes.h"
#include <string>

namespace pycc::codegen {

struct EmitResult {
  std::string llPath;
  std::string asmPath;
  std::string objPath;
  std::string binPath;
};

class Codegen {
 public:
  Codegen(bool emitLL, bool emitASM) : emitLL_(emitLL), emitASM_(emitASM) {}

  // Returns empty error string on success; non-empty error on failure.
  std::string emit(const ast::Module& mod, const std::string& outBase,
                   bool assemblyOnly, bool compileOnly, EmitResult& result) const;

  // For testing: generate LLVM IR text from AST without invoking toolchain.
  static std::string generateIR(const ast::Module& mod);

 private:
  bool emitLL_{true};
  bool emitASM_{true};
  static bool runCmd(const std::string& cmd, std::string& outErr);
};

} // namespace pycc::codegen
