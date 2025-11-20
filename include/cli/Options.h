#pragma once

#include <string>
#include <vector>

#include "ColorMode.h"

namespace pycc::cli {

    enum class AstLogMode {
        None,
        Before,
        After,
        Both
    };

    struct Options {
        bool showHelp{false};
        bool emitAssemblyOnly{false}; // -S
        bool compileOnly{false};      // -c
        bool metrics{false};          // --metrics
        bool metricsJson{false};      // --metrics-json
        bool optConstFold{false};     // --opt-const-fold
        bool optAlgebraic{false};     // --opt-algebraic
        bool optDCE{false};           // --opt-dce
        bool optCFG{false};           // --opt-cfg
        std::string outputFile{"a.out"};
        std::vector<std::string> inputs{};
        std::vector<std::string> defines{}; // -DMACRO[=VALUE]
        ColorMode color{ColorMode::Auto};
        int diagContext{1};
        AstLogMode astLog{AstLogMode::None};
        std::string logPath{"."};    // --log-path=<dir> (defaults to ./)
        bool logLexer{false};         // --log-lexer
        bool logAst{false};           // --log-ast (file logging; not to stdout)
        bool logCodegen{false};       // --log-codegen
    };

} // namespace pycc::cli
