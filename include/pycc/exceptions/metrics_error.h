/***
 * Name: pycc::exceptions::MetricsError
 * Purpose: Exception for metrics collection/reporting issues.
 * Inputs: Error message
 * Outputs: Exception object
 * Theory of Operation: Marker type deriving from PyccException.
 */
#pragma once

#include "pycc/exceptions/pycc_exception.h"

namespace pycc {
namespace exceptions {

class MetricsError : public PyccException {
 public:
  using PyccException::PyccException;
};

}  // namespace exceptions
}  // namespace pycc

