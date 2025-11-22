/**
 * @file
 * @brief AST utility declarations (HasBodyPair mixin).
 */
/***
 * Name: pycc::ast::HasBodyPair
 * Purpose: Mixin for nodes that contain then/else statement lists.
 */
#pragma once

#include <memory>
#include <vector>

namespace pycc::ast {

template <typename StmtT>
struct HasBodyPair {
    std::vector<std::unique_ptr<StmtT>> thenBody;
    std::vector<std::unique_ptr<StmtT>> elseBody;
};

} // namespace pycc::ast
