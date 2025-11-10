/***
 * Name: pycc::exceptions::PyccException
 * Purpose: Base class for all pycc exceptions; do not use built-in exceptions directly.
 * Inputs: Message string describing the error condition
 * Outputs: Exception object providing `what()` text
 * Theory of Operation: Derives from std::exception to interoperate with catch sites,
 *   but all throws in pycc must use a custom type derived from this base.
 */
#pragma once

#include <exception>
#include <string>

namespace pycc {
namespace exceptions {

class PyccException : public std::exception {
 public:
  virtual ~PyccException() noexcept = default;
  const char* what() const noexcept override;

 protected:
  explicit PyccException(std::string msg) noexcept;
  std::string message_;
};

}  // namespace exceptions
}  // namespace pycc

