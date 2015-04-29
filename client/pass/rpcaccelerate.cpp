//    Copyright (c) 2015 University of Paderborn 
//                         (Marvin Damschen <marvin.damschen@gullz.de>,
//                          Gavin Vaz <gavin.vaz@uni-paderborn.de>,
//                          Heinrich Riebler <heinrich.riebler@uni-paderborn.de>)

//    Permission is hereby granted, free of charge, to any person obtaining a copy
//    of this software and associated documentation files (the "Software"), to deal
//    in the Software without restriction, including without limitation the rights
//    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//    copies of the Software, and to permit persons to whom the Software is
//    furnished to do so, subject to the following conditions:

//    The above copyright notice and this permission notice shall be included in
//    all copies or substantial portions of the Software.

//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//    THE SOFTWARE.

// References: LowerSetJmp.cpp (replace function calls)
#define DEBUG_TYPE "rpcacc"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Bitcode/ReaderWriter.h"

#include <unordered_map>
#include <string>
#include <list>
#include <iostream>

#include "rpcaccelerate.h"

using namespace llvm;

namespace {
    class RPCAccelerate : public FunctionPass {
	public:
		static char ID;
        RPCAccelerate() : FunctionPass(ID) {}

        virtual bool runOnFunction(Function &F);
        void setFunctionScores(std::unordered_map<std::string, unsigned long> *scores);
    private:
        std::unordered_map<std::string, unsigned long> *scores;
	};
}

char RPCAccelerate::ID = 0;
static RegisterPass<RPCAccelerate> X("rpcacc", "replaces calls to function to RPC calls", false, false);

FunctionPass *baar::createRPCAcceleratePass(std::unordered_map<std::string, unsigned long> *functionScores) {
    auto Pass = new RPCAccelerate();
    Pass->setFunctionScores(functionScores);

    return Pass;
}

bool RPCAccelerate::runOnFunction(Function &F) {
    // After this procedure, this function will conceptually look like the following ($functionScore is a constant at this point and will be inserted):
    // printf("INFO: runtime decision is $functionScore >= %ld ?\n", totalArgSizeInBytes);
    // if ($functionScore > totalArgSize)
    //    callAcc(func, args);
    // else
    //    oldFunc();

    IRBuilder<> Builder(F.getContext());
    BasicBlock &oldFunctionBegin = F.front();
    BasicBlock *callAccBB = BasicBlock::Create(F.getContext(), "", &F, &(F.front()));
    BasicBlock *ScoreConditionBB = BasicBlock::Create(F.getContext(), "", &F, callAccBB);
    Builder.SetInsertPoint(ScoreConditionBB);

    auto ToBeAcceleratedFunctionType = F.getFunctionType();
    llvm::Value* totalArraySizeInByte = Builder.getInt64(0U);
    long unsigned totalNotArrayArgSize = ToBeAcceleratedFunctionType->getReturnType()->getScalarSizeInBits() / 8;
    auto argI = F.getArgumentList().begin();
    for (auto paramI = ToBeAcceleratedFunctionType->param_begin(); paramI != ToBeAcceleratedFunctionType->param_end(), argI != F.getArgumentList().end(); paramI++, argI++) {
        if ((*paramI)->getTypeID() == llvm::Type::PointerTyID) {
            assert((*(paramI + 1))->getTypeID() == llvm::Type::IntegerTyID); // It is assumed that the parameter after an array is #elements (also when marshalling arrays)
            argI++; // ilist_iterator unfortunately does not support '+' operator
            const auto ArrayNumElementsArgVal = argI;
            argI--;

            auto ArrayElementType = (*paramI)->getSequentialElementType();
            while (ArrayElementType->getTypeID() == llvm::Type::PointerTyID || ArrayElementType->getTypeID() == llvm::Type::ArrayTyID)
                ArrayElementType = ArrayElementType->getSequentialElementType();

            const auto& ArrayElementSize = Builder.getInt64((ArrayElementType->getScalarSizeInBits() / 8) * 2); // count arrays twice (transferred to and from server as a whole)
            const auto& ArrayNumElementsArgExt = Builder.CreateSExtOrTrunc(ArrayNumElementsArgVal, totalArraySizeInByte->getType(), "arrayNumElementsArgExt"); // TODO ZExt ?
            const auto& ArraySizeInByte = Builder.CreateMul(ArrayElementSize, ArrayNumElementsArgExt, "arraySizeInByte");
            totalArraySizeInByte = Builder.CreateAdd(totalArraySizeInByte, ArraySizeInByte, "totalArraySizeInByte");
        } else
            totalNotArrayArgSize += ((*paramI)->getScalarSizeInBits() / 8);
    }
    std::cout << "DEBUG: Total argument size, excluding arrays, in bytes is " << totalNotArrayArgSize << std::endl;

    const auto& totalArgSizeInBytes = Builder.CreateAdd(totalArraySizeInByte, Builder.getInt64(totalNotArrayArgSize), "totalArgSizeInBytes");
    if (F.getParent()->getFunction("printf")) // TODO use getOrInsertFunction(..) to always declare printf and show runtime decision
        Builder.CreateCall2(F.getParent()->getFunction("printf"), Builder.CreateGlobalString("INFO: runtime decision is " + std::to_string(scores->at(F.getName().str())) + " >= %ld ?\n"), totalArgSizeInBytes);
    else
        std::cout << "WARNING: " << "declare printf in program to see runtime decision \n";
    Builder.CreateCondBr(Builder.CreateICmpUGE(Builder.getInt64(scores->at(F.getName().str())), totalArgSizeInBytes), callAccBB, &oldFunctionBegin);
    Builder.SetInsertPoint(callAccBB);

    // The name of the function, as well as the argument types are stored in a string of the form 'RetTy:functionName:ArgTy1:ArgTy2:...:ArgTyn'
    // This way, the typeinfo can be easily reused in the client, whithout parsing the IR
    // return type
    std::string retTypeFunctionNameArgTypes = std::to_string(F.getReturnType()->getTypeID());
    if (F.getReturnType()->getTypeID() == llvm::Type::IntegerTyID) // for ints, we have to add the bitwidth
        retTypeFunctionNameArgTypes += std::string(";") + std::to_string(cast<llvm::IntegerType>(F.getReturnType())->getBitWidth());
    // function name
    retTypeFunctionNameArgTypes += std::string(":") + F.getName().str();
    // arg types
    for (unsigned int i = 0; i < ToBeAcceleratedFunctionType->getNumParams(); i++) {
        llvm::Type* currTy = ToBeAcceleratedFunctionType->getParamType(i);
        retTypeFunctionNameArgTypes += std::string(":") + std::to_string(currTy->getTypeID());

        if (currTy->getTypeID() == llvm::Type::IntegerTyID) // for ints, we have to add the bitwidth
            retTypeFunctionNameArgTypes += std::string(";") + std::to_string(cast<llvm::IntegerType>(currTy)->getBitWidth());

        if (currTy->getTypeID() == llvm::Type::PointerTyID) { // for pointer, we need to add the typeID of type pointed to and dimension for arrays
            auto elementType = cast<llvm::PointerType>(currTy)->getElementType();
            while (elementType->getTypeID() == llvm::Type::ArrayTyID || elementType->getTypeID() == llvm::Type::PointerTyID)
                elementType = cast<llvm::SequentialType>(elementType)->getElementType();

            auto pointedToTyID = elementType->getTypeID();
            retTypeFunctionNameArgTypes += std::string(";") + std::to_string(pointedToTyID);
            if (pointedToTyID == llvm::Type::IntegerTyID) // for ints, we have to add the bitwidth
                retTypeFunctionNameArgTypes += std::string(";") + std::to_string(cast<llvm::IntegerType>(elementType)->getBitWidth());
        }

    }
    std::cout << "DEBUG: RPCAccPass, retTypeFunctionNameArgTypes = \"" + retTypeFunctionNameArgTypes +"\"\n";

    // add functionNameAndArgTypes to IR and set as first argument to the call
    auto ir_functionNameAndArgTypes = Builder.CreateGlobalStringPtr(retTypeFunctionNameArgTypes, ".str");
    std::vector<Value*> callAcc_params;
    callAcc_params.push_back(ir_functionNameAndArgTypes);

    // If return type != void, initialise container of F's return type for return value of rpc call and add to arguments
    llvm::AllocaInst* functionReturnValuePtr;
    if (F.getReturnType()->getTypeID() != llvm::Type::VoidTyID) {
        functionReturnValuePtr = Builder.CreateAlloca(F.getReturnType(), 0, "functionReturnValuePtr");
        callAcc_params.push_back(functionReturnValuePtr);
    }

    // place in F's arguments into callAcc-call
    for (auto& arg : F.getArgumentList())
        callAcc_params.push_back(&arg);

    // create the call to "callAcc" and set attributes
    CallInst* callAcc_call = CallInst::Create(F.getParent()->getFunction("callAcc"), callAcc_params, "", callAccBB);
    callAcc_call->setCallingConv(CallingConv::C);
    callAcc_call->setTailCall(false);
    AttributeSet callAcc_call_AS;
    callAcc_call->setAttributes(callAcc_call_AS);

    // If return type != void, return value obtained by rpc
    if (F.getReturnType()->getTypeID() != llvm::Type::VoidTyID) {
        auto functionReturnValue = Builder.CreateLoad(functionReturnValuePtr, "functionReturnValue");
        Builder.CreateRet(functionReturnValue);
    } else
        Builder.CreateRetVoid();

    // DEBUG
    // F.dump();
    return true;
}

void RPCAccelerate::setFunctionScores(std::unordered_map<std::string, unsigned long> *scores)
{
    this->scores = scores;
}
