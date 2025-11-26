#include "cli/ParseArgsInternals.h"

namespace pycc::cli::detail {
    /***
     * Name: pycc::cli::detail::applySimpleBoolFlags
     * Purpose: Handle flag-only boolean options and set outputs.
     */
    bool applySimpleBoolFlags(std::string_view arg, Options &out) {
        if (isFlag(arg, "-h")) {
            out.showHelp = true;
            return true;
        }
        if (isFlag(arg, "--help")) {
            out.showHelp = true;
            return true;
        }
        if (isFlag(arg, "-S")) {
            out.emitAssemblyOnly = true;
            return true;
        }
        if (isFlag(arg, "-c")) {
            out.compileOnly = true;
            return true;
        }
        if (isFlag(arg, "--metrics")) {
            out.metrics = true;
            return true;
        }
        if (isFlag(arg, "--metrics-json")) {
            out.metricsJson = true;
            return true;
        }
        if (isFlag(arg, "--opt-const-fold")) {
            out.optConstFold = true;
            return true;
        }
        if (isFlag(arg, "--opt-algebraic")) {
            out.optAlgebraic = true;
            return true;
        }
        if (isFlag(arg, "--opt-dce")) {
            out.optDCE = true;
            return true;
        }
        if (isFlag(arg, "--opt-cfg")) {
            out.optCFG = true;
            return true;
        }
        if (isFlag(arg, "--log-lexer")) {
            out.logLexer = true;
            return true;
        }
        if (isFlag(arg, "--log-ast")) {
            out.logAst = true;
            return true;
        }
        if (isFlag(arg, "--log-codegen")) {
            out.logCodegen = true;
            return true;
        }
        if (isFlag(arg, "--ast-log")) {
            out.astLog = AstLogMode::Before;
            return true;
        }
        return false;
    }
} // namespace pycc::cli::detail
