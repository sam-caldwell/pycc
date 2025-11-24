// Integration tests to exercise Sema, TypeEnv, ExpressionTyper, and ReturnParam inference.
#include <gtest/gtest.h>

#include "sema/Sema.h"
#include "sema/TypeEnv.h"
#include "sema/detail/checks/ReturnParamInfer.h"
#include "sema/detail/ExpressionTyper.h"
#include "ast/Nodes.h"

using namespace pycc;

TEST(SemaCoverage, TypeEnvBasics) {
  sema::TypeEnv env;
  env.define("x", ast::TypeKind::Int, {});
  env.unionSet("x", sema::TypeEnv::maskForKind(ast::TypeKind::Float), {});
  ASSERT_TRUE(env.get("x").has_value());

  env.restrictToKind("x", ast::TypeKind::Int);
  ASSERT_EQ(env.get("x").value(), ast::TypeKind::Int);

  env.excludeKind("x", ast::TypeKind::NoneType); // also marks non-none
  ASSERT_TRUE(env.isNonNone("x"));

  env.defineListElems("L", sema::TypeEnv::maskForKind(ast::TypeKind::Int));
  ASSERT_NE(env.getListElems("L"), 0U);

  env.defineTupleElems("T", {sema::TypeEnv::maskForKind(ast::TypeKind::Int), sema::TypeEnv::maskForKind(ast::TypeKind::Str)});
  ASSERT_NE(env.getTupleElemAt("T", 0), 0U);
  ASSERT_NE(env.unionOfTupleElems("T"), 0U);

  env.defineDictKeyVals("D", sema::TypeEnv::maskForKind(ast::TypeKind::Str), sema::TypeEnv::maskForKind(ast::TypeKind::Int));
  ASSERT_NE(env.getDictKeys("D"), 0U);
  ASSERT_NE(env.getDictVals("D"), 0U);

  sema::TypeEnv a, b, dst;
  a.define("v", ast::TypeKind::Int, {});
  b.define("v", ast::TypeKind::Int, {});
  dst.intersectFrom(a, b);
  ASSERT_EQ(dst.get("v").value(), ast::TypeKind::Int);
}

TEST(SemaCoverage, ReturnParamInfer) {
  ast::FunctionDef f("foo", ast::TypeKind::Int);
  ast::Param pa; pa.name = "a"; f.params.emplace_back(std::move(pa));
  ast::Param pb; pb.name = "b"; f.params.emplace_back(std::move(pb));
  // return a
  f.body.emplace_back(std::make_unique<ast::ReturnStmt>(std::make_unique<ast::Name>("a")));
  auto idx = sema::detail::inferReturnParamIdx(f);
  ASSERT_TRUE(idx.has_value());
  EXPECT_EQ(idx.value(), 0);

  // Now add a conflicting return b; inference should fail
  f.body.emplace_back(std::make_unique<ast::ReturnStmt>(std::make_unique<ast::Name>("b")));
  auto idx2 = sema::detail::inferReturnParamIdx(f);
  EXPECT_FALSE(idx2.has_value());
}

TEST(SemaCoverage, ExpressionTyperBasic) {
  // env: x:int
  sema::TypeEnv env;
  env.define("x", ast::TypeKind::Int, {});

  std::unordered_map<std::string, sema::Sig> sigs;
  std::unordered_map<std::string, int> retIdx;
  std::vector<sema::Diagnostic> diags;

  // Name("x") -> Int
  ast::Name xName("x");
  sema::ExpressionTyper t1{env, sigs, retIdx, diags};
  xName.accept(t1);
  ASSERT_TRUE(t1.ok);
  EXPECT_EQ(t1.out, ast::TypeKind::Int);

  // 1 + 2 -> Int
  auto one = std::make_unique<ast::IntLiteral>(1);
  auto two = std::make_unique<ast::IntLiteral>(2);
  ast::Binary add(ast::BinaryOperator::Add, std::move(one), std::move(two));
  sema::ExpressionTyper t2{env, sigs, retIdx, diags};
  add.accept(t2);
  ASSERT_TRUE(t2.ok);
  EXPECT_EQ(t2.out, ast::TypeKind::Int);

  // List literal [1,2]
  auto l = std::make_unique<ast::ListLiteral>();
  l->elements.emplace_back(std::make_unique<ast::IntLiteral>(1));
  l->elements.emplace_back(std::make_unique<ast::IntLiteral>(2));
  sema::ExpressionTyper t3{env, sigs, retIdx, diags};
  l->accept(t3);
  ASSERT_TRUE(t3.ok);
  EXPECT_EQ(t3.out, ast::TypeKind::List);
}

TEST(SemaCoverage, SemaCheckSimpleModule) {
  // def foo(a:int, b:int) -> int: return a
  ast::Module mod;
  auto fn = std::make_unique<ast::FunctionDef>("foo", ast::TypeKind::Int);
  ast::Param pa; pa.name = "a"; pa.type = ast::TypeKind::Int; fn->params.emplace_back(std::move(pa));
  ast::Param pb; pb.name = "b"; pb.type = ast::TypeKind::Int; fn->params.emplace_back(std::move(pb));
  fn->body.emplace_back(std::make_unique<ast::ReturnStmt>(std::make_unique<ast::Name>("a")));
  mod.functions.emplace_back(std::move(fn));

  std::vector<sema::Diagnostic> diags;
  sema::Sema s;
  bool ok = s.check(mod, diags);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(diags.empty());
}
