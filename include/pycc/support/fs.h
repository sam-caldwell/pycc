/***
 * Name: pycc::support (fs)
 * Purpose: Minimal file IO helpers for reading and writing text files.
 * Inputs: Paths and string buffers
 * Outputs: File contents to/from disk
 * Theory of Operation: Thin wrappers over fstream to centralize error handling.
 */
#pragma once

#include <string>

namespace pycc {
namespace support {

/*** ReadFile: Read entire file into out. Return true on success. */
bool ReadFile(const std::string& path, std::string& out, std::string& err);

/*** WriteFile: Write entire string to path. Return true on success. */
bool WriteFile(const std::string& path, const std::string& data, std::string& err);

}  // namespace support
}  // namespace pycc

