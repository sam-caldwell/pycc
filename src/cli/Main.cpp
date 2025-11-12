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
#include "cli/CLI.h"
#include "compiler/Compiler.h"
#include <iostream>

int main(const int argc, char** argv) {
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
  pycc::Compiler c;
  return c.run(opts);
}

