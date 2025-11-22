/***
 * Name: ExpressionTyper::visit(statement nodes)
 * Purpose: Error paths when statement nodes are encountered in expression typing.
 */
#include "sema/detail/ExpressionTyper.h"

using namespace pycc;
using namespace pycc::sema;

void ExpressionTyper::visit(const ast::ExprStmt& es) { addDiag(*diags, "internal error: exprstmt is not expression", &es); ok = false; }
void ExpressionTyper::visit(const ast::ReturnStmt& rs) { addDiag(*diags, "internal error: return is not expression", &rs); ok = false; }
void ExpressionTyper::visit(const ast::AssignStmt& as) { addDiag(*diags, "internal error: assign is not expression", &as); ok = false; }
void ExpressionTyper::visit(const ast::IfStmt& is) { addDiag(*diags, "internal error: if is not expression", &is); ok = false; }
void ExpressionTyper::visit(const ast::FunctionDef& fn) { addDiag(*diags, "internal error: function is not expression", &fn); ok = false; }
void ExpressionTyper::visit(const ast::Module& m) { addDiag(*diags, "internal error: module is not expression", &m); ok = false; }

