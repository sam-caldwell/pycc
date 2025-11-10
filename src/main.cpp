/***
 * Name: pycc::main
 * Purpose: Entry point for the pycc compiler CLI.
 * Inputs:
 *   - argc, argv: Standard process arguments.
 * Outputs:
 *   - int: POSIX process status code (0 on success).
 * Theory of Operation:
 *   Initializes the driver and, in future, will parse CLI flags,
 *   configure compilation, and invoke the frontend/backend. For now,
 *   it prints a placeholder banner and reflects build options.
 */
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "pycc/driver/cli.h"
#include "pycc/driver/app.h"
#include "pycc/exceptions/pycc_exception.h"
#include "pycc/metrics/metrics.h"

#ifndef PYCC_EMIT_LLVM
#define PYCC_EMIT_LLVM 0
#endif
#ifndef PYCC_EMIT_ASM
#define PYCC_EMIT_ASM 0
#endif

using pycc::driver::CliOptions;

int main(int argc, char** argv) {
    try {
    using pycc::driver::ParseCli;
    using pycc::driver::PrintUsage;
    CliOptions opts;
    if (!ParseCli(argc, (const char* const*)argv, opts, std::cerr)) {
        PrintUsage(std::cerr, argv[0]); // NOLINT(*-pro-bounds-pointer-arithmetic)
        return 2;
    }
    if (opts.show_help) {
        PrintUsage(std::cout, argv[0]); // NOLINT(*-pro-bounds-pointer-arithmetic)
        return 0;
    }
    pycc::metrics::Metrics::Enable(opts.metrics);
    if (opts.inputs.size() != 1) { std::cerr << "pycc: error: exactly one input file is supported in MVP" << '\n'; return 2; }
    const int ret_code = pycc::driver::CompileOnce(opts, opts.inputs[0]);
    pycc::driver::ReportMetricsIfRequested(opts);
    return ret_code;
    }
    catch (const pycc::exceptions::PyccException& ex) {
        std::cerr << "pycc: " << ex.what() << '\n';
        return 2;
    }
    catch (const std::exception& ex) {
        std::cerr << "pycc: internal error: " << ex.what() << '\n';
        return 2;
    }
    catch (...) {
        std::cerr << "pycc: unknown error" << '\n';
        return 2;
    }
}
