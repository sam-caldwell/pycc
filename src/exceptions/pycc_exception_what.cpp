/***
 * Name: pycc::exceptions::PyccException::what
 * Purpose: Return the stored error message.
 * Inputs: none
 * Outputs: C-string pointer valid for the lifetime of the exception
 * Theory of Operation: Returns message_.c_str(); noexcept.
 */
#include "pycc/exceptions/pycc_exception.h"

namespace pycc::exceptions {

const char* PyccException::what() const noexcept { return message_.c_str(); }

}  // namespace pycc::exceptions


