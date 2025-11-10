/***
 * Name: pycc::exceptions::ConfigError
 * Purpose: Exception for configuration and option errors.
 * Inputs: Error message
 * Outputs: Exception object
 * Theory of Operation: Marker type deriving from PyccException.
 */
#pragma once

#include "pycc/exceptions/pycc_exception.h"

namespace pycc {
namespace exceptions {

class ConfigError : public PyccException {
 public:
  using PyccException::PyccException;
};

}  // namespace exceptions
}  // namespace pycc

