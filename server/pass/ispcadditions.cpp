/*
  Copyright (c) 2010-2013, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.


   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
   IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
   TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

#include <vector>

static bool lVectorValuesAllEqual(llvm::Value *v, int vectorLength,
                      std::vector<llvm::PHINode *> &seenPhis,
                      llvm::Value **splatValue = nullptr) {
    if (vectorLength == 1)
        return true;

    if (llvm::isa<llvm::ConstantAggregateZero>(v)) {
        if (splatValue) {
            llvm::ConstantAggregateZero *caz =
                llvm::dyn_cast<llvm::ConstantAggregateZero>(v);
            *splatValue = caz->getSequentialElement();
        }
        return true;
    }

    llvm::ConstantVector *cv = llvm::dyn_cast<llvm::ConstantVector>(v);
    if (cv != NULL) {
        llvm::Value* splat = cv->getSplatValue();
        if (splat != NULL && splatValue) {
            *splatValue = splat;
        }
        return (splat != NULL);
    }

    llvm::ConstantDataVector *cdv = llvm::dyn_cast<llvm::ConstantDataVector>(v);
    if (cdv != NULL) {
        llvm::Value* splat = cdv->getSplatValue();
        if (splat != NULL && splatValue) {
            *splatValue = splat;
        }
        return (splat != NULL);
    }

    llvm::BinaryOperator *bop = llvm::dyn_cast<llvm::BinaryOperator>(v);
    if (bop != NULL) {
        // Easy case: both operands are all equal -> return true
        if (lVectorValuesAllEqual(bop->getOperand(0), vectorLength,
                                  seenPhis) &&
            lVectorValuesAllEqual(bop->getOperand(1), vectorLength,
                                  seenPhis))
            return true;

        /*// If it's a shift, take a special path that tries to check if the
        // high (surviving) bits of the values are equal.
        if (bop->getOpcode() == llvm::Instruction::AShr ||
            bop->getOpcode() == llvm::Instruction::LShr)
            return lVectorShiftRightAllEqual(bop->getOperand(0),
                                             bop->getOperand(1), vectorLength);*/

        return false;
    }

    llvm::CastInst *cast = llvm::dyn_cast<llvm::CastInst>(v);
    if (cast != NULL)
        return lVectorValuesAllEqual(cast->getOperand(0), vectorLength,
                                     seenPhis);

    /*llvm::InsertElementInst *ie = llvm::dyn_cast<llvm::InsertElementInst>(v);
    if (ie != NULL) {
        return (LLVMFlattenInsertChain(ie, vectorLength) != NULL);
    }*/

    llvm::PHINode *phi = llvm::dyn_cast<llvm::PHINode>(v);
    if (phi) {
        for (unsigned int i = 0; i < seenPhis.size(); ++i)
            if (seenPhis[i] == phi)
                return true;

        seenPhis.push_back(phi);

        unsigned int numIncoming = phi->getNumIncomingValues();
        // Check all of the incoming values: if all of them are all equal,
        // then we're good.
        for (unsigned int i = 0; i < numIncoming; ++i) {
            if (!lVectorValuesAllEqual(phi->getIncomingValue(i), vectorLength,
                                       seenPhis)) {
                seenPhis.pop_back();
                return false;
            }
        }

        seenPhis.pop_back();
        return true;
    }

    if (llvm::isa<llvm::UndefValue>(v))
        // ?
        return false;

    assert(!llvm::isa<llvm::Constant>(v));

    if (llvm::isa<llvm::CallInst>(v) || llvm::isa<llvm::LoadInst>(v) ||
        !llvm::isa<llvm::Instruction>(v))
        return false;

    llvm::ShuffleVectorInst *shuffle = llvm::dyn_cast<llvm::ShuffleVectorInst>(v);
    if (shuffle != NULL) {
        llvm::Value *indices = shuffle->getOperand(2);
        if (lVectorValuesAllEqual(indices, vectorLength, seenPhis))
            // The easy case--just a smear of the same element across the
            // whole vector.
            return true;

        // TODO: handle more general cases?
        return false;
    }

#if 0
    fprintf(stderr, "all equal: ");
    v->dump();
    fprintf(stderr, "\n");
    llvm::Instruction *inst = llvm::dyn_cast<llvm::Instruction>(v);
    if (inst) {
        inst->getParent()->dump();
        fprintf(stderr, "\n");
        fprintf(stderr, "\n");
    }
#endif

    return false;
}

static bool LLVMVectorValuesAllEqual(llvm::Value *v, llvm::Value **splat = nullptr) {
    llvm::VectorType *vt =
        llvm::dyn_cast<llvm::VectorType>(v->getType());
    assert(vt != NULL);
    int vectorLength = vt->getNumElements();

    std::vector<llvm::PHINode *> seenPhis;
    bool equal = lVectorValuesAllEqual(v, vectorLength, seenPhis, splat);

    return equal;
}
