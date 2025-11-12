#ifndef PYCC_COMPILER_COMPILER_H
#define PYCC_COMPILER_COMPILER_H

/***
 * Name: pycc::Compiler
 * Purpose: Orchestrate the end-to-end pipeline for Milestone 1.
 * Inputs:
 *   - CLI options
 * Outputs:
 *   - Artifacts written to disk; exit code
 * Theory of Operation:
 *   Reads source, lexes, parses, computes geometry, emits IR/ASM/BIN using
 *   Codegen and reports optional metrics.
 */

// Forward declarations to reduce header coupling
namespace pycc { namespace cli { struct Options; } }
namespace pycc { namespace sema { struct Diagnostic; } }

namespace pycc {
    class Compiler {
    public:
        static int run(const cli::Options &opts);

        static bool use_env_color();

        static void print_error(const sema::Diagnostic &diag, bool color, int context);
    };
} // namespace pycc

#endif // PYCC_COMPILER_COMPILER_H
