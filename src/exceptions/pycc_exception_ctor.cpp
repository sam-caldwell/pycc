/***
 * Name: pycc::exceptions::PyccException::PyccException
 * Purpose: Construct base exception with a message.
 * Inputs:
 *   - msg: human-readable error description
 * Outputs: Initialized exception object
 * Theory of Operation: Stores the message for later retrieval by what().
 */
#include "pycc/exceptions/pycc_exception.h"

namespace pycc {
namespace exceptions {

PyccException::PyccException(std::string msg) noexcept : message_(std::move(msg)) {}

}  // namespace exceptions
}  // namespace pycc

