/***
 * Name: pycc::stages::FileReader::Read
 * Purpose: Read a file from disk and record metrics for ReadFile phase.
 * Inputs:
 *   - path: file path
 * Outputs:
 *   - out_src: populated with contents on success
 *   - err: error message on failure
 * Theory of Operation: Uses support::ReadFile; timings via Metrics::ScopedTimer.
 */
#include "pycc/stages/file_reader.h"

#include "pycc/metrics/metrics.h"  // direct use of Metrics::ScopedTimer
#include "pycc/support/fs.h"

#include <string>

namespace pycc::stages {

auto FileReader::Read(const std::string& path, std::string& out_src, std::string& err) -> bool {
  const metrics::Metrics::ScopedTimer timer(metrics::Metrics::Phase::ReadFile);
  return support::ReadFile(path, out_src, err);
}

}  // namespace pycc::stages
