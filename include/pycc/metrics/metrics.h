/***
 * Name: pycc::metrics::Metrics
 * Purpose: OO metrics interface with static registry. Compiler stages can inherit
 *   this class and use ScopedTimer plus helper methods to record metrics.
 * Inputs: Phase identifiers and payloads (AST geometry, optimization notes)
 * Outputs: A static registry accessible by the application for reporting.
 * Theory of Operation: All instances share a static Registry and enabled flag.
 */
#pragma once

#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "pycc/ast/ast.h"

namespace pycc {

namespace metrics {

class Metrics {
 public:
  enum class Phase { ReadFile, Parse, Sema, EmitIR, EmitASM, Compile, Link };

  struct Registry {
    bool enabled{false};
    std::vector<std::pair<Phase, std::uint64_t>> durations_ns;
    ast::ASTGeometry ast_geom{};
    std::vector<std::string> optimizations;
  };

  class ScopedTimer {
   public:
    explicit ScopedTimer(Phase phase)
        : phase_(phase), start_(std::chrono::steady_clock::now()) {}
    ~ScopedTimer() noexcept {
      if (!reg_.enabled) return;
      auto end = std::chrono::steady_clock::now();
      auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
      reg_.durations_ns.emplace_back(phase_, static_cast<std::uint64_t>(ns));
    }

   private:
    Phase phase_;
    std::chrono::time_point<std::chrono::steady_clock> start_;
  };

  static void Enable(bool on) { reg_.enabled = on; }
  static Registry& GetRegistry() { return reg_; }
  static void RecordOptimization(std::string note) { if (reg_.enabled) reg_.optimizations.emplace_back(std::move(note)); }
  static void SetASTGeometry(const ast::ASTGeometry& g) { if (reg_.enabled) reg_.ast_geom = g; }

  static void PrintMetrics(const Registry& reg, std::ostream& out);
  static void PrintMetricsJson(const Registry& reg, std::ostream& out);

 protected:
  Metrics() = default;

 private:
  static Registry reg_;
};

}  // namespace metrics
}  // namespace pycc
