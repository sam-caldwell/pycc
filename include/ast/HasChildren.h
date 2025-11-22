/**
 * @file
 * @brief AST utility declarations (HasChildren mixin).
 */
#pragma once

#include <memory>
#include <vector>

namespace pycc::ast {

template <typename ChildT>
struct HasChildren {
    std::vector<std::unique_ptr<ChildT>> children;
};

}
