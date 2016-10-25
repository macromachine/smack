//
// This file is distributed under the MIT License. See LICENSE for details.
//

#define DEBUG_TYPE "codify-static-inits"

#include "smack/Naming.h"
#include "smack/SmackOptions.h"
#include "smack/CodifyStaticInits.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Regex.h"
#include "llvm/IR/DataLayout.h"

#include <deque>
#include <vector>

namespace smack {

using namespace llvm;

namespace{
  Regex STRING_CONSTANT("^\\.str[.0-9]*$");
}

bool CodifyStaticInits::runOnModule(Module& M) {
  TD = &M.getDataLayout();
  LLVMContext& C = M.getContext();

  Function* F = dyn_cast<Function>(
    M.getOrInsertFunction(Naming::STATIC_INIT_PROC,
      Type::getVoidTy(C), NULL));

  BasicBlock* B = BasicBlock::Create(C, "entry", F);
  IRBuilder<> IRB(B);

  std::deque< std::tuple< Constant*, Constant*, std::vector<Value*> > > worklist;

  for (auto &G : M.globals())
    if (G.hasInitializer())
      if (!G.hasName() || !STRING_CONSTANT.match(G.getName().str()))
        worklist.push_back(std::make_tuple(
          G.getInitializer(), &G, std::vector<Value*>()));

  while (worklist.size()) {
    Constant* V = std::get<0>(worklist.front());
    Constant* P = std::get<1>(worklist.front());
    std::vector<Value*> I = std::get<2>(worklist.front());
    worklist.pop_front();

    if (V->getType()->isIntegerTy() ||
        V->getType()->isPointerTy() ||
        V->getType()->isFloatingPointTy())

      IRB.CreateStore(V, IRB.CreateGEP(P, ArrayRef<Value*>(I)));

    else if (ArrayType* AT = dyn_cast<ArrayType>(V->getType()))
      for (unsigned i = AT->getNumElements(); i-- > 0; ) {
        auto A = V->getAggregateElement(i);
        std::vector<Value*> idxs(I);
        if (idxs.empty())
          idxs.push_back(ConstantInt::get(Type::getInt32Ty(C),0));
        idxs.push_back(ConstantInt::get(Type::getInt64Ty(C),i));
        worklist.push_front(std::make_tuple(A, P, std::vector<Value*>(idxs)));
      }

    else if (StructType* ST = dyn_cast<StructType>(V->getType()))
      for (unsigned i = ST->getNumElements(); i-- > 0; ) {
        auto A = V->getAggregateElement(i);
        std::vector<Value*> idxs(I);
        if (idxs.empty())
          idxs.push_back(ConstantInt::get(Type::getInt32Ty(C),0));
        idxs.push_back(ConstantInt::get(Type::getInt32Ty(C),i));
        worklist.push_front(std::make_tuple(A, P, std::vector<Value*>(idxs)));
      }

    else if (V->getType()->isX86_FP80Ty())
      errs() << "warning: ignored X86 FP80 initializer" << "\n";

    else
      assert (false && "Unexpected static initializer.");
  }

  IRB.CreateRetVoid();

  return true;
}

// Pass ID variable
char CodifyStaticInits::ID = 0;

// Register the pass
static RegisterPass<CodifyStaticInits>
X("codify-static-inits", "Codify Static Initializers");

}
