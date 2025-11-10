/***
 * Name: pycc::backend::detail::ExecAndWait
 * Purpose: Exec a command via execvp and wait for completion; capture error.
 * Inputs: argv (mutable, null-terminated), err (out)
 * Outputs: true on success (exit 0), false and err on failure
 * Theory of Operation: POSIX fork/exec/wait; assembles a display command on error.
 */
#include "pycc/backend/detail/exec.h"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace pycc {
namespace backend {
namespace detail {

auto ExecAndWait(std::vector<char*>& argv, std::string& err) -> bool {  // NOLINT(readability-function-size)
  const auto pid = fork();
  if (pid < 0) {
    err = "failed to fork() for clang";
    return false;
  }
  if (pid == 0) {
    execvp(argv[0], argv.data());
    constexpr int kExecFailure = 127;
    _exit(kExecFailure);
  }
  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    err = "failed to waitpid() for clang";
    return false;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::ostringstream assembled;
    for (std::size_t i = 0; argv[i] != nullptr; ++i) {
      if (i != 0U) {
        assembled << ' ';
      }
      assembled << argv[i];
    }
    constexpr int kUnknownExitCode = -1;
    const int code = WIFEXITED(status) ? WEXITSTATUS(status) : kUnknownExitCode;
    err = "clang invocation failed (rc=" + std::to_string(code) + "): " + assembled.str();
    return false;
  }
  return true;
}

}  // namespace detail
}  // namespace backend
}  // namespace pycc
