/**
 * Name: pycc::lex::FileInput
 * Purpose: File-backed input source implementation.
 */
#pragma once

#include <istream>
#include <memory>
#include <string>
#include "lexer/InputSource.h"

namespace pycc::lex {

class FileInput : public InputSource {
public:
    explicit FileInput(std::string path);

    bool getline(std::string& out) override;

    const std::string& name() const override { return path_; }

private:
    std::string path_{};
    std::unique_ptr<std::istream> in_{nullptr};
};

} // namespace pycc::lex

