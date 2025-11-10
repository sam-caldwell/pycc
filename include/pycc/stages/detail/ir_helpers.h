/***
 * Name: pycc::stages::detail (IR helpers)
 * Purpose: Small helpers to reduce complexity in IR emitter.
 * Inputs: AST root or source text
 * Outputs: Optional return integer literal
 * Theory of Operation: Walk minimal AST shape or parse fallback from text.
 */
#pragma once

#include <optional>
#include <string>

#include "pycc/ast/ast.h"

namespace pycc {
namespace stages {
namespace detail {

/*** FindReturnIntLiteral: Search AST for main's return int literal. */
std::optional<int> FindReturnIntLiteral(const ast::Node& root);

/*** ParseReturnIntFromSource: Fallback string parse of 'return <int>'. */
std::optional<int> ParseReturnIntFromSource(const std::string& src);

}  // namespace detail
}  // namespace stages
}  // namespace pycc

