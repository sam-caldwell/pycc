#include "cli/Usage.h"
#include <string>
#include <string_view>
namespace pycc::cli {

namespace {
// Keep help text as a compile-time constant to avoid reallocation work.
constexpr std::string_view kUsageText = R"(pycc [options] file...

Options:
  -h, --help           Print this help and exit
  -o <file>            Place the output into <file> (default: a.out)
  -S                   Compile only; generate assembly (do not link)
  -c                   Compile and assemble (object file); do not link
  --metrics            Print compilation metrics summary
  --metrics-json       Print compilation metrics in JSON
  --opt-const-fold     Enable constant-folding optimizer pass (experimental)
  --opt-algebraic      Enable algebraic simplification optimizer pass
  --opt-dce            Enable dead-code elimination pass (trailing returns)
  --ast-log[=<mode>]   Dump AST: before|after|both (default: before)
  --log-path=<dir>     Directory where logs are written (lexer/ast/codegen)
  --log-lexer          Enable lexer token log (requires --log-path)
  --log-ast            Enable AST file logs (requires --log-path)
  --log-codegen        Enable codegen IR log (requires --log-path)
  --color=<mode>       Color diagnostics: always|never|auto (default: auto)
  --diag-context=<N>   Lines of context to show around errors (default: 1)
  --                    End of options
)";
} // namespace

std::string Usage() { return std::string(kUsageText); }
} // namespace pycc::cli
