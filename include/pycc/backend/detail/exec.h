/***
 * Name: pycc::backend::detail (exec helpers)
 * Purpose: Internal helpers to build argv and exec/wait for clang.
 * Inputs: Vector<string> for argv construction; argv for exec; error string
 * Outputs: Mutable argv pointer array; success/failure boolean with error text
 * Theory of Operation: Keep backend driver small to satisfy lint and structure rules.
 */
#pragma once

#include <string>
#include <vector>

namespace pycc {
namespace backend {
namespace detail {

/*** BuildArgvMutable: Build null-terminated argv pointers referencing args storage. */
std::vector<char*> BuildArgvMutable(std::vector<std::string>& args);

/*** ExecAndWait: fork/execvp and wait; returns true on exit code 0; else fills err. */
bool ExecAndWait(std::vector<char*>& argv, std::string& err);

}  // namespace detail
}  // namespace backend
}  // namespace pycc

