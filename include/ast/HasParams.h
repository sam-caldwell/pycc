/**
 * @file
 * @brief AST utility declarations (HasParams mixin).
 */
#pragma once

#include <vector>

namespace pycc::ast {

template <typename ParamT>
struct HasParams {
    std::vector<ParamT> params;
};

}
