/**
 * @file
 * @brief AST utility declarations (HasName mixin).
 */
#pragma once

#include <string>

namespace pycc::ast {

struct HasName {
    std::string name;
};

}
