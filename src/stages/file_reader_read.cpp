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

#include "pycc/support/fs.h"

namespace pycc {
namespace stages {

bool FileReader::Read(const std::string& path, std::string& out_src, std::string& err) {
  metrics::Metrics::ScopedTimer t(metrics::Metrics::Phase::ReadFile);
  return support::ReadFile(path, out_src, err);
}

}  // namespace stages
}  // namespace pycc

