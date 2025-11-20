/***
 * Name: pycc::Compiler::run
 * Purpose: Execute Milestone 1 pipeline end-to-end.
 */
#include "compiler/Compiler.h"
#include "ast/GeometrySummary.h"
#include "ast/Module.h"
#include "cli/ColorMode.h"
#include "cli/Options.h"
#include "codegen/Codegen.h"
#include "lexer/Lexer.h"
#include "observability/AstPrinter.h"
#include "observability/Metrics.h"
#include "runtime/Runtime.h"
#include "optimizer/AlgebraicSimplify.h"
#include "optimizer/ConstantFold.h"
#include "optimizer/LocalProp.h"
#include "optimizer/DCE.h"
#include "optimizer/SimplifyCFG.h"
#include "optimizer/Optimizer.h"
#include "parser/Parser.h"
#include "sema/Sema.h"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

namespace pycc {

// removed in favor of streaming lexer input

int Compiler::run(const cli::Options& opts) { // NOLINT(readability-function-size,readability-function-cognitive-complexity)
  if (opts.inputs.empty()) {
    std::cerr << "pycc: no input files provided\n";
    return 2;
  }
  const std::string input = opts.inputs.front();

  obs::Metrics metrics;
  metrics.start("Lex");
  lex::Lexer lexer; // NOLINT(misc-const-correctness)
  lexer.pushFile(input);
  metrics.stop("Lex");

  // Optional log directory creation
  bool logsEnabled = true;
  const std::string logDir = opts.logPath.empty() ? std::string(".") : opts.logPath;
  if (!logDir.empty()) {
    std::error_code errCode;
    namespace fs = std::filesystem;
    if (!fs::exists(logDir, errCode)) {
      if (!fs::create_directories(logDir, errCode) && !fs::exists(logDir)) {
        std::cerr << "pycc: failed to create log directory '" << logDir << "': " << errCode.message() << "\n";
        logsEnabled = false;
      }
    }
  }

  std::unique_ptr<ast::Module> mod;
  try {
    metrics.start("Parse");
    parse::Parser parser(lexer);
    mod = parser.parseModule();
    metrics.stop("Parse");
    // Counters after parse
    if (mod) {
      metrics.setCounter("parse.functions", static_cast<uint64_t>(mod->functions.size()));
      metrics.setCounter("parse.classes", static_cast<uint64_t>(mod->classes.size()));
    }
  } catch (const std::exception& ex) {
    sema::Diagnostic pd;
    pd.file = input;
    pd.line = 1; pd.col = 1;
    pd.message = std::string("parse error: ") + ex.what();
    bool color = false;
    if (opts.color == cli::ColorMode::Always) { color = true; }
    else if (opts.color == cli::ColorMode::Never) { color = false; }
    else { constexpr int kStderrFd = 2; color = (isatty(kStderrFd) != 0) || use_env_color(); }
    print_error(pd, color, opts.diagContext);
    return 1;
  }

  auto geom = ast::ComputeGeometry(*mod);
  metrics.setAstGeometry({geom.nodes, geom.maxDepth});

  // Sema
  metrics.start("Sema");
  sema::Sema sema; // NOLINT(misc-const-correctness)
  std::vector<sema::Diagnostic> diags;
  const bool semaOk = sema.check(*mod, diags);
  metrics.stop("Sema");
  metrics.setGauge("sema.ok", semaOk ? 1U : 0U);
  metrics.setCounter("sema.diagnostics", static_cast<uint64_t>(diags.size()));
  if (!semaOk) {
    bool color = false;
    if (opts.color == cli::ColorMode::Always) { color = true; }
    else if (opts.color == cli::ColorMode::Never) { color = false; }
    else {
      constexpr int kStderrFd = 2;
      color = (isatty(kStderrFd) != 0) || use_env_color();
    }
    for (const auto& diag : diags) { print_error(diag, color, opts.diagContext); }
    return 1;
  }

  // Timestamp prefix for logs
  auto tsNow = std::chrono::system_clock::now();
  const std::time_t tsTime = std::chrono::system_clock::to_time_t(tsNow);
  std::tm tmBuf{};
#ifdef _WIN32
  localtime_s(&tmBuf, &tsTime);
#else
  localtime_r(&tsTime, &tmBuf);
#endif
  std::ostringstream timestampStream;
  timestampStream << std::put_time(&tmBuf, "%Y%m%d-%H%M%S");
  const std::string tsPrefix = timestampStream.str() + "-";

  // Optional AST logging (before optimization)
  if (opts.astLog == cli::AstLogMode::Before || opts.astLog == cli::AstLogMode::Both) {
    obs::AstPrinter printer; // NOLINT(misc-const-correctness)
    const auto out = printer.print(*mod);
    std::cout << "== AST (before opt) ==\n" << out;
    if (logsEnabled && opts.logAst) {
      std::ofstream beforeAst(logDir + "/" + tsPrefix + "ast.before.ast.log");
      beforeAst << out;
    }
  }

  // Optional optimizer analysis pass (non-mutating; visitor-based)
  metrics.start("OptimizeAST");
  const opt::Optimizer optimizer;
  (void)optimizer.analyze(*mod);
  metrics.stop("OptimizeAST");

  if (opts.optConstFold) {
    metrics.start("ConstFold");
    opt::ConstantFold constFold; // NOLINT(misc-const-correctness)
    auto folds = constFold.run(*mod);
    metrics.setOptimizerStat("folds", static_cast<uint64_t>(folds));
    metrics.setCounter("opt.constfold", static_cast<uint64_t>(folds));
    for (const auto& [key, count] : constFold.stats()) { metrics.incOptimizerBreakdown("constfold", key, count); }
    metrics.stop("ConstFold");
  }

  if (opts.optConstFold || opts.optAlgebraic) {
    metrics.start("LocalProp");
    opt::LocalProp lp; // NOLINT(misc-const-correctness)
    auto props = lp.run(*mod);
    metrics.setOptimizerStat("localprop", static_cast<uint64_t>(props));
    metrics.stop("LocalProp");
  }

  if (opts.optAlgebraic) {
    metrics.start("Algebraic");
    opt::AlgebraicSimplify algebraic; // NOLINT(misc-const-correctness)
    auto rewrites = algebraic.run(*mod);
    metrics.setOptimizerStat("algebraic", static_cast<uint64_t>(rewrites));
    metrics.setCounter("opt.algebraic", static_cast<uint64_t>(rewrites));
    for (const auto& [key, count] : algebraic.stats()) { metrics.incOptimizerBreakdown("algebraic", key, count); }
    metrics.stop("Algebraic");
  }

  if (opts.optCFG) {
    metrics.start("CFG");
    opt::SimplifyCFG cfg; // NOLINT(misc-const-correctness)
    auto pruned = cfg.run(*mod);
    metrics.setOptimizerStat("cfg_pruned", static_cast<uint64_t>(pruned));
    metrics.setCounter("opt.cfg_pruned", static_cast<uint64_t>(pruned));
    for (const auto& [key, count] : cfg.stats()) { metrics.incOptimizerBreakdown("cfg", key, count); }
    metrics.stop("CFG");
  }

  if (opts.optDCE) {
    metrics.start("DCE");
    opt::DCE dce; // NOLINT(misc-const-correctness)
    auto removed = dce.run(*mod);
    metrics.setOptimizerStat("dce_removed", static_cast<uint64_t>(removed));
    metrics.setCounter("opt.dce_removed", static_cast<uint64_t>(removed));
    for (const auto& [key, count] : dce.stats()) { metrics.incOptimizerBreakdown("dce", key, count); }
    metrics.stop("DCE");
  }

  // Optional AST logging (after optimization)
  if (opts.astLog == cli::AstLogMode::After || opts.astLog == cli::AstLogMode::Both) {
    obs::AstPrinter printer2; // NOLINT(misc-const-correctness)
    const auto out = printer2.print(*mod);
    std::cout << "== AST (after opt) ==\n" << out;
    if (logsEnabled && opts.logAst) {
      std::ofstream afterAst(logDir + "/" + tsPrefix + "ast.after.ast.log");
      afterAst << out;
    }
  }

  metrics.start("EmitIR");
  const codegen::Codegen codegenDriver(true, true);
  codegen::EmitResult res;
  const std::string outBase = opts.outputFile;
  // Provide the source path to codegen for embedding source comments in IR output
#ifdef __APPLE__
  setenv("PYCC_SOURCE_PATH", input.c_str(), 1);
#else
  setenv("PYCC_SOURCE_PATH", input.c_str(), 1);
#endif
  // Enable optional LLVM IR pass to elide GC barriers on stack writes via -DOPT_ELIDE_GCBARRIER
  auto hasDefine = [&](const std::string& key) {
    for (const auto& d : opts.defines) {
      if (d == key || d.rfind(key + "=", 0) == 0) return true;
    }
    return false;
  };
  if (hasDefine("OPT_ELIDE_GCBARRIER")) {
    setenv("PYCC_OPT_ELIDE_GCBARRIER", "1", /*overwrite*/1);
  }
  const std::string err = codegenDriver.emit(*mod, outBase, opts.emitAssemblyOnly, opts.compileOnly, res);
  metrics.stop("EmitIR");
  if (!err.empty()) {
    std::cerr << "pycc: codegen error: " << err << "\n";
    return 1;
  }

  // Record lexer token count and IR sizes when metrics requested
  auto recordPost = [&]() {
    try {
      const auto toks = lexer.tokens();
      metrics.setCounter("lex.tokens", static_cast<uint64_t>(toks.size()));
    } catch (...) { /* ignore */ }
    try {
      auto irText = codegen::Codegen::generateIR(*mod);
      metrics.setGauge("codegen.ir_bytes", static_cast<uint64_t>(irText.size()));
      uint64_t lines = 0; for (char c : irText) if (c == '\n') ++lines; metrics.setGauge("codegen.ir_lines", lines);
    } catch (...) { /* ignore */ }
  };
  if (opts.metrics || opts.metricsJson) { recordPost(); }

  // Runtime perf counters/telemetry snapshot
  if (opts.metrics || opts.metricsJson) {
    auto st = rt::gc_stats();
    auto telem = rt::gc_telemetry();
    metrics.setGauge("rt.bytes_live", st.bytesLive);
    metrics.setGauge("rt.bytes_allocated", st.bytesAllocated);
    metrics.setCounter("rt.collections", st.numCollections);
    // Store telemetry as integer gauges for stability in JSON
    metrics.setGauge("rt.alloc_rate_bps", static_cast<uint64_t>(telem.allocRateBytesPerSec >= 0 ? telem.allocRateBytesPerSec : 0.0));
    metrics.setGauge("rt.pressure_ppm", static_cast<uint64_t>((telem.pressure >= 0 ? telem.pressure : 0.0) * 1000000.0));
  }

  if (logsEnabled) {
    // Lexer log
    if (opts.logLexer) {
      std::ofstream lexFile(logDir + "/" + tsPrefix + "lexer.lex.log");
      for (const auto& tok : lexer.tokens()) {
        lexFile << tok.file << ":" << tok.line << ":" << tok.col << " " << to_string(tok.kind) << " " << tok.text << "\n";
      }
    }
    // Codegen log (IR)
    if (opts.logCodegen) {
      try {
        auto irText = codegen::Codegen::generateIR(*mod);
        // Prepend original source as IR comments using the same env var as Codegen::emit
        if (const char* srcPath = std::getenv("PYCC_SOURCE_PATH"); srcPath && *srcPath) {
          std::ifstream inSrc(srcPath);
          if (inSrc) {
            std::ostringstream commented;
            commented << "; ---- PY SOURCE: " << srcPath << " ----\n";
            std::string line;
            while (std::getline(inSrc, line)) { commented << "; " << line << "\n"; }
            commented << "; ---- END PY SOURCE ----\n\n";
            commented << irText;
            irText = commented.str();
          }
        }
        std::ofstream codegenFile(logDir + "/" + tsPrefix + "codegen.codegen.log");
        codegenFile << irText;
      } catch (...) { /* no-op: best-effort IR log */ } // NOLINT(bugprone-empty-catch)
    }
  }

  // Emit metrics at end of build:
  // - With --metrics-json: JSON only
  // - With --metrics: include human-readable text, then JSON for tool consumption
  if (opts.metricsJson) {
    std::cout << metrics.summaryJson();
  } else if (opts.metrics) {
    std::cout << metrics.summaryText();
    std::cout << metrics.summaryJson();
  }

  // Always write metrics JSON to log directory when any metrics requested
  if (opts.metrics || opts.metricsJson) {
    const auto jsonSummary = metrics.summaryJson();
    const std::string metricsPath = logsEnabled
        ? (logDir + "/" + tsPrefix + "metrics.json")
        : (std::string("./") + tsPrefix + std::string("metrics.json"));
    std::ofstream metricsFile(metricsPath);
    metricsFile << jsonSummary;
  }
  return 0;
}

} // namespace pycc
