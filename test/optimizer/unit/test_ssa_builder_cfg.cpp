// Tests for SSABuilder CFG: conditionals and loops produce joins and back-edges,
// and phi placeholders are added at join points.

#include <gtest/gtest.h>

#include "optimizer/SSABuilder.h"

#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/WhileStmt.h"
#include "ast/ForStmt.h"
#include "ast/AssignStmt.h"
#include "ast/IntLiteral.h"

using namespace pycc::ast;
using namespace pycc::opt;

static std::unique_ptr<AssignStmt> makeAssign(const std::string& name, long long val) {
  return std::make_unique<AssignStmt>(name, std::make_unique<IntLiteral>(val));
}

TEST(SSABuilderCFG, IfJoinProducesPhiForVarAssignedInBothBranches) {
  FunctionDef fn{"f", TypeKind::NoneType};
  auto ifs = std::make_unique<IfStmt>(std::make_unique<IntLiteral>(1));
  ifs->thenBody.emplace_back(makeAssign("x", 1));
  ifs->elseBody.emplace_back(makeAssign("x", 2));
  fn.body.emplace_back(std::move(ifs));

  SSABuilder b;
  auto ssa = b.build(fn);

  // Find a join block (two preds) that has a phi for x
  bool foundPhi = false;
  for (const auto& bb : ssa.blocks) {
    if (bb.pred.size() >= 2) {
      for (const auto& phi : bb.phis) {
        if (phi.var == "x") { foundPhi = true; break; }
      }
    }
    if (foundPhi) break;
  }
  EXPECT_TRUE(foundPhi);
}

TEST(SSABuilderCFG, WhileHeaderHasBackEdgeAndPhiWithPreheader) {
  FunctionDef fn{"g", TypeKind::NoneType};
  fn.body.emplace_back(makeAssign("x", 0)); // preheader def
  auto ws = std::make_unique<WhileStmt>(std::make_unique<IntLiteral>(1));
  ws->thenBody.emplace_back(makeAssign("x", 1)); // body def
  fn.body.emplace_back(std::move(ws));

  SSABuilder b;
  auto ssa = b.build(fn);

  // Find while header block
  int headerId = -1;
  for (const auto& bb : ssa.blocks) {
    for (const auto* s : bb.stmts) {
      if (s && s->kind == NodeKind::WhileStmt) { headerId = bb.id; break; }
    }
    if (headerId >= 0) break;
  }
  ASSERT_GE(headerId, 0);
  // Header should have >=2 preds (preheader + latch)
  EXPECT_GE(ssa.blocks[headerId].pred.size(), 2u);
  // Header should have a phi for x
  bool foundPhi = false;
  for (const auto& phi : ssa.blocks[headerId].phis) if (phi.var == "x") { foundPhi = true; break; }
  EXPECT_TRUE(foundPhi);
}

TEST(SSABuilderCFG, ForHeaderHasBackEdge) {
  FunctionDef fn{"h", TypeKind::NoneType};
  auto fs = std::make_unique<ForStmt>(std::make_unique<IntLiteral>(0), std::make_unique<IntLiteral>(3));
  fs->thenBody.emplace_back(makeAssign("y", 9));
  fn.body.emplace_back(std::move(fs));

  SSABuilder b; auto ssa = b.build(fn);
  int headerId = -1;
  for (const auto& bb : ssa.blocks) {
    for (const auto* s : bb.stmts) if (s && s->kind == NodeKind::ForStmt) { headerId = bb.id; break; }
    if (headerId >= 0) break;
  }
  ASSERT_GE(headerId, 0);
  EXPECT_GE(ssa.blocks[headerId].pred.size(), 1u);
  // There should be a back-edge from some successor path
  bool hasBack = false;
  for (int p : ssa.blocks[headerId].pred) if (p != headerId) {
    for (int s : ssa.blocks[p].succ) if (s == headerId) { hasBack = true; break; }
    if (hasBack) break;
  }
  EXPECT_TRUE(hasBack);
}

