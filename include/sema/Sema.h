#pragma once

#include "ast/Nodes.h"
#include "sema/Diagnostic.h"
#include "sema/FuncFlags.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace pycc::sema {
    /***
     * Name: pycc::sema::Sema
     * Purpose: Perform minimal type checking and name/arity validation before codegen.
     * Inputs:
     *   - AST Module
     * Outputs:
     *   - true/false with diagnostics messages
     * Theory of Operation:
     *   Gathers function signatures, then checks each function body:
     *   - Names must be defined before use
     *   - Binary ops require int operands and yield int
     *   - Calls must resolve to known functions with matching arity and int args
     *   - Return expressions must match declared return type
     */
    class Sema {
    public:
        // Annotates expression nodes with inferred TypeKind.
        bool check(ast::Module &mod, std::vector<Diagnostic> &diags);

        const std::unordered_map<const ast::FunctionDef *, FuncFlags> &functionFlags() const { return funcFlags_; }

        bool mayRaise(const ast::Stmt *s) const {
            const auto it = stmtMayRaise_.find(s);
            return (it != stmtMayRaise_.end()) ? it->second : false;
        }

    private:
        // Allow the implementation helper to access private fields during the refactor.
        friend bool sema_check_impl(Sema *self, ast::Module &mod, std::vector<Diagnostic> &diags);

        std::unordered_map<const ast::FunctionDef *, FuncFlags> funcFlags_;
        std::unordered_map<const ast::Stmt *, bool> stmtMayRaise_;
    };
} // namespace pycc::sema
