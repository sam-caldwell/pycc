// SPDX-License-Identifier: MIT
// ElideGCBarrierPass: Remove calls to pycc_gc_write_barrier for stack (alloca) writes.

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

static bool originatesFromAlloca(Value *V) {
  SmallVector<Value *, 16> worklist{V};
  SmallVector<Value *, 16> seen;
  while (!worklist.empty()) {
    Value *X = worklist.pop_back_val();
    if (llvm::is_contained(seen, X)) continue;
    seen.push_back(X);

    X = X->stripPointerCasts();
    if (isa<AllocaInst>(X)) return true;

    if (auto *GEP = dyn_cast<GetElementPtrInst>(X)) {
      worklist.push_back(GEP->getPointerOperand());
      continue;
    }
    if (auto *PN = dyn_cast<PHINode>(X)) {
      for (Value *In : PN->incoming_values()) worklist.push_back(In);
      continue;
    }
    if (auto *Sel = dyn_cast<SelectInst>(X)) {
      worklist.push_back(Sel->getTrueValue());
      worklist.push_back(Sel->getFalseValue());
      continue;
    }
    // Other producers: be conservative (not from alloca).
  }
  return false;
}

struct ElideGCBarrierPass : public PassInfoMixin<ElideGCBarrierPass> {
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    SmallVector<CallBase *, 8> toErase;
    for (Instruction &I : instructions(F)) {
      auto *CB = dyn_cast<CallBase>(&I);
      if (!CB) continue;
      Function *Callee = CB->getCalledFunction();
      if (!Callee) continue; // skip indirect
      if (Callee->getName() != "pycc_gc_write_barrier") continue;
      if (CB->arg_size() < 1) continue;
      Value *Addr = CB->getArgOperand(0);
      if (originatesFromAlloca(Addr)) {
        toErase.push_back(CB);
      }
    }
    for (CallBase *CB : toErase) CB->eraseFromParent();
    return toErase.empty() ? PreservedAnalyses::all() : PreservedAnalyses::none();
  }
};

} // namespace

// New PassManager plugin entry point
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  static ::llvm::PassPluginLibraryInfo Info{
      LLVM_PLUGIN_API_VERSION, "PyccPasses", LLVM_VERSION_STRING,
      [](PassBuilder &PB) {
        // Allow selecting via -passes=function(pycc-elide-gcbarrier)
        PB.registerPipelineParsingCallback(
            [](StringRef Name, FunctionPassManager &FPM, ArrayRef<PassBuilder::PipelineElement>) {
              if (Name == "pycc-elide-gcbarrier") {
                FPM.addPass(ElideGCBarrierPass());
                return true;
              }
              return false;
            });

        // Also attach at the end of optimizer pipeline for convenience.
        PB.registerOptimizerLastEPCallback(
            [](ModulePassManager &MPM, OptimizationLevel) {
              FunctionPassManager FPM;
              FPM.addPass(ElideGCBarrierPass());
              MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
            });
      }};
  return Info;
}

