/**
 * Name: pycc::lex::StringInput
 * Purpose: String-backed input source implementation.
 */
#pragma once

#include <istream>
#include <memory>
#include <string>
#include "lexer/InputSource.h"

namespace pycc::lex {

class StringInput : public InputSource {
public:
    StringInput(std::string text, std::string name);

    bool getline(std::string& out) override;

    const std::string& name() const override { return name_; }

private:
    std::string name_{};
    std::unique_ptr<std::istream> in_{nullptr};
};

} // namespace pycc::lex

