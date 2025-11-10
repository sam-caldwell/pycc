/***
 * Name: pycc::exceptions::BackendError
 * Purpose: Exception for backend codegen/link failures.
 * Inputs: Error message
 * Outputs: Exception object
 * Theory of Operation: Marker type deriving from PyccException.
 */
#pragma once

#include "pycc/exceptions/pycc_exception.h"

namespace pycc {
namespace exceptions {

class BackendError : public PyccException {
 public:
  using PyccException::PyccException;
};

}  // namespace exceptions
}  // namespace pycc

