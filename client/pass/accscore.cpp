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

#include "accscore.h"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/RegionInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Support/CommandLine.h"

#include "polly/ScopDetection.h"

#include <iostream>

char baar::AccScore::ID = 0;
static RegisterPass<baar::AccScore> X("scoring", "scores functions to decide which functions to offload to accelerator from candidates", false, false);

llvm::cl::opt<int> IOPsWeight("iop-weight", llvm::cl::desc("Weight to multiply to integer operation count when scoring functions, defaults to 1"), llvm::cl::init(1));
llvm::cl::opt<int> FLOPsWeight("flop-weight", llvm::cl::desc("Weight to multiply to floating point operationg count when scoring functions, defaults to 1"), llvm::cl::init(1));

FunctionPass *baar::createScoringPass() {
    return new AccScore();
}

bool baar::AccScore::runOnFunction(Function &F) {
    polly::ScopDetection &SCoPDetect = getAnalysis<polly::ScopDetection>();
    if (SCoPDetect.begin() == SCoPDetect.end()) {
        score = 0;
        std::cout << "INFO: " << "no SCoP detected in " << F.getName().str() << std::endl;
    } else {
        LoopInfo &LoopInf = getAnalysis<LoopInfo>();
        for (auto Region = SCoPDetect.begin(); Region != SCoPDetect.end(); Region++) {
            unsigned long regionScore = 0;
            for (LoopInfo::iterator Loop = LoopInf.begin(); Loop != LoopInf.end(); Loop++) {
                unsigned long LoopInnermostTotalTripCount = getInnermostTotalTripCount(**Loop);

                unsigned totalLoopFLOPs = 0;
                unsigned totalLoopIOPs = 0;
                for (const auto& Block : (*Loop)->getBlocks()) {
                    visit(Block);
                    totalLoopFLOPs += lastBB_FLOPs;
                    totalLoopIOPs += lastBB_IOPs;
                }
                std::cout << "DEBUG: " << " LoopInnermostTotalTripCount = " << LoopInnermostTotalTripCount << ", totalLoopIOPs = " << totalLoopIOPs << ", totalLoopFLOPs = " << totalLoopFLOPs << std::endl;
                unsigned long loopScore = LoopInnermostTotalTripCount * (IOPsWeight*totalLoopIOPs + FLOPsWeight*totalLoopFLOPs);
                regionScore += loopScore;
            }
            score += regionScore;
            if (score < regionScore) { // check for overflow
                std::cout << "WARNING: " << "score overflow, set to maximum value \n";
                score = std::numeric_limits<decltype(score)>::max();
                break;
            }
        }        
    }
    std::cout << "DEBUG: " << F.getName().str() << " got a score of " << score << std::endl;

    return false;
}

void baar::AccScore::getAnalysisUsage(AnalysisUsage &AU) const
{
    AU.setPreservesAll();
    AU.addRequired<LoopInfo>();
    AU.addRequired<ScalarEvolution>();
    AU.addRequired<polly::ScopDetection>();
}

unsigned long baar::AccScore::getScore()
{
    return score;
}

unsigned long baar::AccScore::getInnermostTotalTripCount(const Loop &L)
{
    ScalarEvolution &ScalarEv = getAnalysis<ScalarEvolution>();

    unsigned long LTripCount = 1; // neutral value for multiplication below
    if (ScalarEv.hasLoopInvariantBackedgeTakenCount(&L)) {
        const SCEVConstant *ExitCount = dyn_cast<SCEVConstant>(ScalarEv.getBackedgeTakenCount(&L));

        if (ExitCount != nullptr)
            LTripCount = ExitCount->getValue()->getZExtValue();
    } else {
        std::cout << "INFO: " << "Scalar Evolution was unable to detect a loop invariant in loop with ID " << L.getLoopID() << std::endl;
        return 0;
    }

    auto &SubLoops = L.getSubLoops();
    if (SubLoops.size() == 0) {
        return LTripCount;
    } else {
        unsigned long subTripCountTotal = 0;
        for (const auto& SubLoop : SubLoops) {
            auto currSubTripCount = getInnermostTotalTripCount(*SubLoop);

            subTripCountTotal += currSubTripCount;

            if (subTripCountTotal < currSubTripCount)
                return std::numeric_limits<decltype(subTripCountTotal)>::max();
        }
        subTripCountTotal *= LTripCount;
        if (subTripCountTotal < LTripCount)
            return std::numeric_limits<decltype(subTripCountTotal)>::max();
        else
            return subTripCountTotal;
    }
}
