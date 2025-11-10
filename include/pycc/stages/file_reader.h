/***
 * Name: pycc::stages::FileReader
 * Purpose: Stage class for reading an input source file.
 * Inputs: Filesystem path
 * Outputs: Source string
 * Theory of Operation: Wraps support::ReadFile and instruments metrics via RAII.
 */
#pragma once

#include <string>

#include "pycc/metrics/metrics.h"

namespace pycc {
namespace stages {

class FileReader : public metrics::Metrics {
 public:
  /*** Read: Read file at path into out_src. */
  static bool Read(const std::string& path, std::string& out_src, std::string& err);
};

}  // namespace stages
}  // namespace pycc
