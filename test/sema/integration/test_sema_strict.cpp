// Strict typing tests to ensure Sema enforces hints and inferred types.
#include <gtest/gtest.h>

#include "sema/Sema.h"
#include "sema/Diagnostic.h"
#include "ast/Nodes.h"

using namespace pycc;

static std::unique_ptr<ast::FunctionDef> makeFn(const char* name, ast::TypeKind ret) {
  auto fn = std::make_unique<ast::FunctionDef>(name, ret);
  return fn;
}

TEST(SemaStrict, ReturnTypeMismatchRejected) {
  ast::Module mod;
  auto fn = makeFn("foo", ast::TypeKind::Int);
  // return "s"
  fn->body.emplace_back(std::make_unique<ast::ReturnStmt>(std::make_unique<ast::StringLiteral>(std::string("s"))));
  mod.functions.emplace_back(std::move(fn));
  std::vector<sema::Diagnostic> diags;
  sema::Sema s;
  bool ok = s.check(mod, diags);
  EXPECT_FALSE(ok);
}

TEST(SemaStrict, CallArgumentTypeMismatchRejected) {
  // def f(a:int) -> int: return a
  ast::Module mod;
  auto f = makeFn("f", ast::TypeKind::Int);
  ast::Param pa; pa.name = "a"; pa.type = ast::TypeKind::Int; f->params.emplace_back(std::move(pa));
  f->body.emplace_back(std::make_unique<ast::ReturnStmt>(std::make_unique<ast::Name>("a")));
  mod.functions.emplace_back(std::move(f));
  // def g() -> int: return f("x")
  auto g = makeFn("g", ast::TypeKind::Int);
  auto call = std::make_unique<ast::Call>(std::make_unique<ast::Name>("f"));
  call->args.emplace_back(std::make_unique<ast::StringLiteral>(std::string("x")));
  g->body.emplace_back(std::make_unique<ast::ReturnStmt>(std::move(call)));
  mod.functions.emplace_back(std::move(g));
  std::vector<sema::Diagnostic> diags;
  sema::Sema s;
  bool ok = s.check(mod, diags);
  EXPECT_FALSE(ok);
}

