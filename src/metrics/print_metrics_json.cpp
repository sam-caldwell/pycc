/***
 * Name: pycc::metrics::PrintMetricsJson
 * Purpose: Print metrics in JSON for consumption by tools.
 * Inputs:
 *   - reg: metrics registry
 *   - out: destination stream
 * Outputs: None
 * Theory of Operation: Simple JSON writer; escapes strings and prints arrays of objects.
 */
#include "pycc/metrics/metrics.h"

#include <cstddef>
#include <iomanip>
#include <ios>
#include <ostream>
#include <sstream>
#include <string>

namespace pycc::metrics {

static const char* PhaseName(Metrics::Phase phase) {
  switch (phase) {
    case Metrics::Phase::ReadFile: return "ReadFile";
    case Metrics::Phase::Parse: return "Parse";
    case Metrics::Phase::Sema: return "Sema";
    case Metrics::Phase::EmitIR: return "EmitIR";
    case Metrics::Phase::EmitASM: return "EmitASM";
    case Metrics::Phase::Compile: return "Compile";
    case Metrics::Phase::Link: return "Link";
  }
  return "Unknown";
}

static std::string JsonEscape(const std::string& str) {  // NOLINT(readability-function-size)
  std::string out;
  constexpr std::size_t kReservePadding = 8;
  out.reserve(str.size() + kReservePadding);
  for (const unsigned char uchar : str) {
    switch (uchar) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        constexpr unsigned char kMinPrintable = 0x20;
        if (uchar < kMinPrintable) {
          std::ostringstream hex;
          hex << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
              << static_cast<int>(uchar);
          out += hex.str();
        } else {
          out += static_cast<char>(uchar);
        }
    }
  }
  return out;
}

auto Metrics::PrintMetricsJson(const Registry& reg, std::ostream& out) -> void {
  if (!reg.enabled) {
    return;
  }
  out << "{";
  // durations
  out << "\n  \"durations_ns\": [";
  for (size_t i = 0; i < reg.durations_ns.size(); ++i) {
    const auto& item = reg.durations_ns[i];
    out << (i != 0U ? ",\n    {" : "\n    {")
        << R"("phase": ")" << PhaseName(item.first) << R"(", "ns": )" << item.second << "}";
  }
  out << "\n  ],";
  // AST
  out << "\n  \"ast\": { \"nodes\": " << reg.ast_geom.node_count
      << ", \"max_depth\": " << reg.ast_geom.max_depth << " },";
  // optimizations
  out << "\n  \"optimizations\": [";
  for (size_t i = 0; i < reg.optimizations.size(); ++i) {
    out << (i != 0U ? ", " : " ") << "\"" << JsonEscape(reg.optimizations[i]) << "\"";
  }
  out << " ]\n}";
}

}  // namespace pycc::metrics
