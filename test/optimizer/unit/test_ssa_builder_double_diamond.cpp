/***
 * Name: test_ssa_builder_double_diamond
 * Purpose: Ensure phi-placement occurs at both joins in a double-diamond CFG for a variable.
 */
#include <gtest/gtest.h>
#include "optimizer/SSABuilder.h"

#include "ast/FunctionDef.h"
#include "ast/IfStmt.h"
#include "ast/AssignStmt.h"
#include "ast/IntLiteral.h"

using namespace pycc::ast;
using namespace pycc::opt;

static std::unique_ptr<AssignStmt> makeAssign(const std::string& name, long long val) {
  return std::make_unique<AssignStmt>(name, std::make_unique<IntLiteral>(val));
}

TEST(SSABuilderCFG, DoubleDiamondProducesTwoPhisForX) {
  FunctionDef fn{"f", TypeKind::NoneType};
  // First diamond
  auto if1 = std::make_unique<IfStmt>(std::make_unique<IntLiteral>(1));
  if1->thenBody.emplace_back(makeAssign("x", 1));
  if1->elseBody.emplace_back(makeAssign("x", 2));
  fn.body.emplace_back(std::move(if1));
  // Second diamond
  auto if2 = std::make_unique<IfStmt>(std::make_unique<IntLiteral>(1));
  if2->thenBody.emplace_back(makeAssign("x", 3));
  if2->elseBody.emplace_back(makeAssign("x", 4));
  fn.body.emplace_back(std::move(if2));

  SSABuilder b;
  auto ssa = b.build(fn);
  int phiBlocksForX = 0;
  int verifiedPhiPredMaps = 0;
  for (const auto& bb : ssa.blocks) {
    if (bb.pred.size() >= 2) {
      for (const auto& phi : bb.phis) {
        if (phi.var == "x") {
          ++phiBlocksForX;
          // Verify phi incomings correspond to predecessors that define x
          size_t good = 0;
          for (int p : phi.incomings) {
            ASSERT_GE(p, 0);
            ASSERT_LT(p, static_cast<int>(ssa.blocks.size()));
            if (ssa.blocks[p].defs.count("x")) { ++good; }
          }
          // At least two preds should define x (both sides of diamond)
          EXPECT_GE(good, 2u);
          ++verifiedPhiPredMaps;
          break;
        }
      }
    }
  }
  EXPECT_GE(phiBlocksForX, 2);
  EXPECT_GE(verifiedPhiPredMaps, 2);
}
