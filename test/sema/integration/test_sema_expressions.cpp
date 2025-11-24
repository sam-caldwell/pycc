// Cover a broad set of ExpressionTyper handlers to drive sema coverage up.
#include <gtest/gtest.h>

#include "sema/detail/ExpressionTyper.h"
#include "sema/TypeEnv.h"
#include "sema/detail/Types.h"
#include "sema/Diagnostic.h"
#include "ast/Nodes.h"

using namespace pycc;

static void run_expr(ast::Expr& e, sema::TypeEnv& env, ast::TypeKind& outK, uint32_t& outSet, bool& ok) {
  std::unordered_map<std::string, sema::Sig> sigs;
  std::unordered_map<std::string, int> retIdx;
  std::vector<sema::Diagnostic> diags;
  sema::ExpressionTyper t{env, sigs, retIdx, diags};
  e.accept(t);
  outK = t.out; outSet = t.outSet; ok = t.ok;
}

TEST(SemaExpressions, SubscriptsAndCalls) {
  sema::TypeEnv env;
  // list L[int]
  env.define("L", ast::TypeKind::List, {});
  env.defineListElems("L", sema::TypeEnv::maskForKind(ast::TypeKind::Int));
  // tuple T[Int, Str]
  env.define("T", ast::TypeKind::Tuple, {});
  env.defineTupleElems("T", {sema::TypeEnv::maskForKind(ast::TypeKind::Int), sema::TypeEnv::maskForKind(ast::TypeKind::Str)});
  // dict D[str->int]
  env.define("D", ast::TypeKind::Dict, {});
  env.defineDictKeyVals("D", sema::TypeEnv::maskForKind(ast::TypeKind::Str), sema::TypeEnv::maskForKind(ast::TypeKind::Int));

  // L[0]
  ast::Subscript sL(std::make_unique<ast::Name>("L"), std::make_unique<ast::IntLiteral>(0));
  ast::TypeKind k; uint32_t set; bool ok;
  run_expr(sL, env, k, set, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(k, ast::TypeKind::Int);

  // [1,2][1]
  auto lst = std::make_unique<ast::ListLiteral>(); lst->elements.emplace_back(std::make_unique<ast::IntLiteral>(1)); lst->elements.emplace_back(std::make_unique<ast::IntLiteral>(2));
  ast::Subscript sLlit(std::move(lst), std::make_unique<ast::IntLiteral>(1));
  run_expr(sLlit, env, k, set, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(k, ast::TypeKind::Int);

  // "abc"[0]
  ast::Subscript sStr(std::make_unique<ast::StringLiteral>(std::string("abc")), std::make_unique<ast::IntLiteral>(0));
  run_expr(sStr, env, k, set, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(k, ast::TypeKind::Str);

  // T[0]
  ast::Subscript sT(std::make_unique<ast::Name>("T"), std::make_unique<ast::IntLiteral>(0));
  run_expr(sT, env, k, set, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(k, ast::TypeKind::Int);

  // (1, "a")[1]
  auto tup = std::make_unique<ast::TupleLiteral>(); tup->elements.emplace_back(std::make_unique<ast::IntLiteral>(1)); tup->elements.emplace_back(std::make_unique<ast::StringLiteral>(std::string("a")));
  ast::Subscript sTlit(std::move(tup), std::make_unique<ast::IntLiteral>(1));
  run_expr(sTlit, env, k, set, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(k, ast::TypeKind::Str);

  // D["key"]
  ast::Subscript sD(std::make_unique<ast::Name>("D"), std::make_unique<ast::StringLiteral>(std::string("key")));
  run_expr(sD, env, k, set, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(k, ast::TypeKind::Int);

  // len([1,2])
  ast::Call cLen(std::make_unique<ast::Name>("len"));
  auto lst2 = std::make_unique<ast::ListLiteral>(); lst2->elements.emplace_back(std::make_unique<ast::IntLiteral>(1)); lst2->elements.emplace_back(std::make_unique<ast::IntLiteral>(2));
  cLen.args.emplace_back(std::move(lst2));
  run_expr(cLen, env, k, set, ok);
  ASSERT_TRUE(ok);
  EXPECT_EQ(k, ast::TypeKind::Int);
}

TEST(SemaExpressions, BinaryUnaryComprehensions) {
  sema::TypeEnv env;
  std::unordered_map<std::string, sema::Sig> sigs;
  std::unordered_map<std::string, int> retIdx;
  std::vector<sema::Diagnostic> diags;

  // a + b, 1 < 2, a & b, a and b
  ast::Binary add(ast::BinaryOperator::Add, std::make_unique<ast::IntLiteral>(1), std::make_unique<ast::IntLiteral>(2));
  sema::ExpressionTyper t1{env, sigs, retIdx, diags}; add.accept(t1); ASSERT_TRUE(t1.ok); EXPECT_EQ(t1.out, ast::TypeKind::Int);

  ast::Binary lt(ast::BinaryOperator::Lt, std::make_unique<ast::IntLiteral>(1), std::make_unique<ast::IntLiteral>(2));
  sema::ExpressionTyper t2{env, sigs, retIdx, diags}; lt.accept(t2); ASSERT_TRUE(t2.ok); EXPECT_EQ(t2.out, ast::TypeKind::Bool);

  ast::Binary band(ast::BinaryOperator::BitAnd, std::make_unique<ast::IntLiteral>(3), std::make_unique<ast::IntLiteral>(1));
  sema::ExpressionTyper t3{env, sigs, retIdx, diags}; band.accept(t3); ASSERT_TRUE(t3.ok); EXPECT_EQ(t3.out, ast::TypeKind::Int);

  ast::Binary land(ast::BinaryOperator::And, std::make_unique<ast::BoolLiteral>(true), std::make_unique<ast::BoolLiteral>(false));
  sema::ExpressionTyper t4{env, sigs, retIdx, diags}; land.accept(t4); ASSERT_TRUE(t4.ok); EXPECT_EQ(t4.out, ast::TypeKind::Bool);

  // -1, not True
  ast::Unary neg(ast::UnaryOperator::Neg, std::make_unique<ast::IntLiteral>(1));
  sema::ExpressionTyper t5{env, sigs, retIdx, diags}; neg.accept(t5); ASSERT_TRUE(t5.ok); EXPECT_EQ(t5.out, ast::TypeKind::Int);

  ast::Unary lnot(ast::UnaryOperator::Not, std::make_unique<ast::BoolLiteral>(true));
  sema::ExpressionTyper t6{env, sigs, retIdx, diags}; lnot.accept(t6); ASSERT_TRUE(t6.ok); EXPECT_EQ(t6.out, ast::TypeKind::Bool);

  // ListComp: [y for y in [1,2] if True]
  ast::ListComp lc; lc.elt = std::make_unique<ast::Name>("y");
  ast::ComprehensionFor f; f.target = std::make_unique<ast::Name>("y");
  auto litList = std::make_unique<ast::ListLiteral>(); litList->elements.emplace_back(std::make_unique<ast::IntLiteral>(1)); litList->elements.emplace_back(std::make_unique<ast::IntLiteral>(2));
  f.iter = std::move(litList); f.ifs.emplace_back(std::make_unique<ast::BoolLiteral>(true)); lc.fors.push_back(std::move(f));
  sema::ExpressionTyper t7{env, sigs, retIdx, diags}; lc.accept(t7); ASSERT_TRUE(t7.ok); EXPECT_EQ(t7.out, ast::TypeKind::List);

  // SetComp
  ast::SetComp sc; sc.elt = std::make_unique<ast::Name>("y");
  ast::ComprehensionFor f2; f2.target = std::make_unique<ast::Name>("y");
  auto litList2 = std::make_unique<ast::ListLiteral>(); litList2->elements.emplace_back(std::make_unique<ast::IntLiteral>(1)); litList2->elements.emplace_back(std::make_unique<ast::IntLiteral>(2));
  f2.iter = std::move(litList2); sc.fors.push_back(std::move(f2));
  sema::ExpressionTyper t8{env, sigs, retIdx, diags}; sc.accept(t8); ASSERT_TRUE(t8.ok); EXPECT_EQ(t8.out, ast::TypeKind::List);

  // DictComp: {k: v for (k,v) in [(1,2)]}
  ast::DictComp dc; dc.key = std::make_unique<ast::Name>("k"); dc.value = std::make_unique<ast::Name>("v");
  ast::ComprehensionFor f3; auto kvList = std::make_unique<ast::ListLiteral>();
  auto tpl = std::make_unique<ast::TupleLiteral>(); tpl->elements.emplace_back(std::make_unique<ast::IntLiteral>(1)); tpl->elements.emplace_back(std::make_unique<ast::IntLiteral>(2));
  kvList->elements.emplace_back(std::move(tpl)); f3.iter = std::move(kvList);
  auto targTpl = std::make_unique<ast::TupleLiteral>(); targTpl->elements.emplace_back(std::make_unique<ast::Name>("k")); targTpl->elements.emplace_back(std::make_unique<ast::Name>("v"));
  f3.target = std::move(targTpl); dc.fors.push_back(std::move(f3));
  sema::ExpressionTyper t9{env, sigs, retIdx, diags}; dc.accept(t9); ASSERT_TRUE(t9.ok); EXPECT_EQ(t9.out, ast::TypeKind::Dict);

  // IfExpr: 1 if True else 2
  ast::IfExpr ife(std::make_unique<ast::IntLiteral>(1), std::make_unique<ast::BoolLiteral>(true), std::make_unique<ast::IntLiteral>(2));
  sema::ExpressionTyper t10{env, sigs, retIdx, diags}; ife.accept(t10); ASSERT_TRUE(t10.ok); EXPECT_EQ(t10.out, ast::TypeKind::Int);
}
