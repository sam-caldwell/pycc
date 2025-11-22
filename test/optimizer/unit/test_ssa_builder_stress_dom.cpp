/***
 * Name: test_ssa_builder_stress_dom
 * Purpose: Stress SSABuilder with nested control flow and validate dominators.
 */
#include <gtest/gtest.h>
#include "optimizer/SSABuilder.h"
#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/WhileStmt.h"
#include "ast/ReturnStmt.h"
#include "ast/AssignStmt.h"
#include "ast/IntLiteral.h"

using namespace pycc::ast;
using namespace pycc::opt;

static std::unique_ptr<AssignStmt> asn(const std::string& n, long long v){ return std::make_unique<AssignStmt>(n, std::make_unique<IntLiteral>(v)); }

TEST(SSABuilderStress, NestedDiamondWithLoopDominators) {
  FunctionDef fn{"stress", TypeKind::NoneType};
  // if 1:
  //   while 1:
  //     x = 1
  //   y = 2
  // else:
  //   y = 3
  auto topIf = std::make_unique<IfStmt>(std::make_unique<IntLiteral>(1));
  {
    auto ws = std::make_unique<WhileStmt>(std::make_unique<IntLiteral>(1));
    ws->thenBody.emplace_back(asn("x", 1));
    topIf->thenBody.emplace_back(std::move(ws));
    topIf->thenBody.emplace_back(asn("y", 2));
  }
  topIf->elseBody.emplace_back(asn("y", 3));
  fn.body.emplace_back(std::move(topIf));
  fn.body.emplace_back(std::make_unique<ReturnStmt>(std::make_unique<IntLiteral>(0)));

  SSABuilder b; auto ssa = b.build(fn);
  // Basic sanity: we should have multiple blocks, and some with >= 2 preds (join), some with back-edges
  ASSERT_GE(ssa.blocks.size(), 5u);
  int ifCond = -1, whileHead = -1, join = -1;
  for (const auto& bb : ssa.blocks) {
    for (const auto* s : bb.stmts) {
      if (s && s->kind == NodeKind::IfStmt) ifCond = bb.id;
      if (s && s->kind == NodeKind::WhileStmt) whileHead = bb.id;
    }
    if (bb.pred.size() >= 2) join = bb.id;
  }
  ASSERT_GE(ifCond, 0);
  ASSERT_GE(whileHead, 0);
  ASSERT_GE(join, 0);

  // Dominator tree expectations: ifCond dominated by entry (0); whileHead and join are reachable and dominated by some block
  auto dt = b.computeDominators(ssa);
  ASSERT_EQ(dt.idom.size(), ssa.blocks.size());
  EXPECT_EQ(dt.idom[ifCond], 0) << "if-cond idom should be entry";
  EXPECT_GE(dt.idom[whileHead], 0);
  EXPECT_GE(dt.idom[join], 0);
}
