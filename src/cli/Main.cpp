#include "compiler/Compiler.h"
#include "cli/ParseArgs.h"
#include "cli/Usage.h"
#include <cstddef>
#include <iostream>
/***
 * Name: pycc::main
 * Purpose: CLI entry point for pycc compiler.
 * Inputs:
 *   - argv
 * Outputs:
 *   - Exit status
 * Theory of Operation:
 *   Parse args then invoke Compiler::run.
 */
int main(const int argc, char** argv) {
  try {
    pycc::cli::Options opts;
    if (!pycc::cli::ParseArgs(argc, argv, opts)) {
      std::cerr << "pycc: argument parse error\n";
      std::cerr << pycc::cli::Usage();
      return 2;
    }
    if (opts.showHelp) {
      std::cout << pycc::cli::Usage();
      return 0;
    }
    return pycc::Compiler::run(opts);
  } catch (...) {
    std::cerr << "pycc: unhandled exception\n";
    return 1;
  }
}
