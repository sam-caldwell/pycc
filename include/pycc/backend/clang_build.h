/***
 * Name: pycc::backend (clang_build)
 * Purpose: Drive the system clang to assemble, compile, and link from LLVM IR.
 * Inputs: IR path, output path, build kind
 * Outputs: Artifacts on disk; success/failure boolean and error text
 * Theory of Operation: Shells out to `clang` with `-S`, `-c`, or default link.
 */
#pragma once

#include <string>

namespace pycc::backend {

    enum class BuildKind { AssembleOnly, ObjectOnly, Link };

    /*** ClangFromIR: Invoke clang for IR->asm/obj/bin. Returns true on success. */
    bool ClangFromIR(const std::string& ir_path,
                     const std::string& output,
                     BuildKind kind,
                     std::string& err,
                     const std::string& clang = "clang");

}
