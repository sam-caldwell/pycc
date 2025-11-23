/**
 * @file
 * @brief validateClassMethod: Enforce dunder method contracts inside classes.
 */
#include "sema/detail/checks/ValidateClassMethod.h"
#include "sema/detail/helpers/AddDiag.h"

namespace pycc::sema::detail {

void validateClassMethod(const ast::FunctionDef* fn,
                         const std::string& className,
                         std::vector<Diagnostic>& diags) {
  if (!fn) return;
  const std::string& name = fn->name;
  if (name == "__init__" && fn->returnType != ast::TypeKind::NoneType) {
    addDiag(diags, std::string("__init__ must return NoneType in class: ") + className, fn);
    return;
  }
  if (name == "__len__" && fn->returnType != ast::TypeKind::Int) {
    addDiag(diags, std::string("__len__ must return int in class: ") + className, fn);
    return;
  }
  if (name == "__get__") {
    const size_t n = fn->params.size();
    if (!(n == 2 || n == 3)) {
      addDiag(diags, std::string("__get__ must take 2 or 3 params in class: ") + className, fn);
    }
    return;
  }
  if (name == "__set__") {
    const size_t n = fn->params.size();
    if (n != 3) {
      addDiag(diags, std::string("__set__ must take exactly 3 params in class: ") + className, fn);
    }
    return;
  }
  if (name == "__delete__") {
    const size_t n = fn->params.size();
    if (n != 2) {
      addDiag(diags, std::string("__delete__ must take exactly 2 params in class: ") + className, fn);
    }
    return;
  }
  if (name == "__getattr__") {
    const size_t n = fn->params.size();
    if (n != 2) {
      addDiag(diags, std::string("__getattr__ must take exactly 2 params in class: ") + className, fn);
    }
    return;
  }
  if (name == "__getattribute__") {
    const size_t n = fn->params.size();
    if (n != 2) { addDiag(diags, std::string("__getattribute__ must take exactly 2 params in class: ") + className, fn); }
    return;
  }
  if (name == "__setattr__") {
    const size_t n = fn->params.size();
    if (n != 3) { addDiag(diags, std::string("__setattr__ must take exactly 3 params in class: ") + className, fn); }
    return;
  }
  if (name == "__delattr__") {
    const size_t n = fn->params.size();
    if (n != 2) { addDiag(diags, std::string("__delattr__ must take exactly 2 params in class: ") + className, fn); }
    return;
  }
  if (name == "__bool__" && fn->returnType != ast::TypeKind::Bool) {
    addDiag(diags, std::string("__bool__ must return bool in class: ") + className, fn);
    return;
  }
  if ((name == "__str__" || name == "__repr__") && fn->returnType != ast::TypeKind::Str) {
    addDiag(diags, name + std::string(" must return str in class: ") + className, fn);
    return;
  }
}

} // namespace pycc::sema::detail
