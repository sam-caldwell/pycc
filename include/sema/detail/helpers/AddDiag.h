/***
 * Name: addDiag
 * Purpose: Append a diagnostic message with optional source location.
 */
#pragma once

#include <string>
#include <vector>
#include "sema/Diagnostic.h"
#include "ast/Node.h"

namespace pycc::sema {
void addDiag(std::vector<Diagnostic>& diags, const std::string& msg, const ast::Node* n);
}

