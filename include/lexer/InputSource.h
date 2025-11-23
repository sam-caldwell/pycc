/**
 * Name: pycc::lex::InputSource
 * Purpose: Abstract line-oriented input source.
 */
#pragma once

#include <string>

namespace pycc::lex {

class InputSource {
public:
    virtual ~InputSource() = default;

    virtual bool getline(std::string& out) = 0; // returns false on EOF
    virtual const std::string& name() const = 0;
};

} // namespace pycc::lex

