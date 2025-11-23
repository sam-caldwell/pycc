/***
 * Name: addDiag
 * Purpose: Append a diagnostic message with optional source location to the vector.
 */
#include "sema/detail/Helpers.h"

namespace pycc::sema {
    void addDiag(std::vector<Diagnostic> &diags, const std::string &msg, const ast::Node *n) {
        Diagnostic diag;
        diag.message = msg;
        if (n != nullptr) {
            diag.file = n->file;
            diag.line = n->line;
            diag.col = n->col;
        }
        diags.push_back(std::move(diag));
    }
} // namespace pycc::sema
