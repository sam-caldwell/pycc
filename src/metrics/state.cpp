/***
 * Name: pycc::metrics::Metrics::reg_
 * Purpose: Define the static metrics registry storage.
 * Inputs: N/A
 * Outputs: Singleton-style storage for metrics across stages.
 * Theory of Operation: One definition for the inline-declared static member.
 */
#include "pycc/metrics/metrics.h"

namespace pycc {
namespace metrics {

Metrics::Registry Metrics::reg_{};

}  // namespace metrics
}  // namespace pycc

