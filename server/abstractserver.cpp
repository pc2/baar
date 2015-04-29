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

#include "abstractserver.h"

#include <iostream>
#include <functional>
#include <chrono>

#include "polly/LinkAllPasses.h"

// Note: LLVM 3.3+ is assumed (released 17th June 2013 or later)
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/Module.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PassNameParser.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Vectorize.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetLibraryInfo.h"

#include "jitbackend.h"
#include "interpreterbackend.h"
#include "extcompilerbackend.h"

llvm::cl::opt<bool> DisableVectorization("disable-vectorization", llvm::cl::desc("Disable vectorization passes during optimization"), llvm::cl::init(false));
llvm::cl::opt<bool> DisablePolly("disable-polly", llvm::cl::desc("Disable Polly passes during optimization"), llvm::cl::init(false));
llvm::cl::opt<bool> DumpOptOut("dump-opt-out", llvm::cl::desc("Dump optimized Module to console"), llvm::cl::init(false));

AbstractServer::AbstractServer(backendTypes backendType) : backendType(backendType)
{    
    LLVMInitializeNativeTarget();
}

AbstractServer::~AbstractServer()
{

}

void AbstractServer::start()
{
    initCommunication();
    handleCommunication();
}

void AbstractServer::end()
{
    cleanupCommunication();
}

std::unique_ptr<AbstractBackend> AbstractServer::parseIRtoBackend(const char *ir_buffer)
{
    // parse received IR from buffer, create ExecutionEngine
    llvm::SMDiagnostic Err;
    llvm::Module *Mod = llvm::ParseIR(llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(ir_buffer, strlen(ir_buffer))), Err, llvm::getGlobalContext());
    if (!Mod) {
        Err.print("acc_server", llvm::errs());
        exit(1);
    }
    llvm::verifyModule(*Mod, llvm::PrintMessageAction);

    const auto StartTimeOpt = std::chrono::high_resolution_clock::now();
    optimizeModule(Mod);
    const auto EndTimeOpt = std::chrono::high_resolution_clock::now();
    TimeDiffOpt = std::chrono::duration_cast<std::chrono::microseconds>(EndTimeOpt - StartTimeOpt);
    std::cout << "INFO: optimization took " << TimeDiffOpt.count() << " microseconds\n";

    const auto StartTimeInit = std::chrono::high_resolution_clock::now();
    std::unique_ptr<AbstractBackend> backend;
    switch(backendType) {
        case extcompiler: backend.reset(new ExtCompilerBackend(Mod)); break;
        case jit: backend.reset(new JITBackend(Mod)); break;
        case interpret: backend.reset(new InterpreterBackend(Mod)); break;
    }
    const auto EndTimeInit = std::chrono::high_resolution_clock::now();
    TimeDiffInit = std::chrono::duration_cast<std::chrono::microseconds>(EndTimeInit - StartTimeInit);
    std::cout << "INFO: Backend was initialized in " << TimeDiffInit.count() << " microseconds \n";

    return backend;
}

llvm::TargetMachine* AbstractServer::GetTargetMachine(llvm::Triple TheTriple) {
  std::string Error;
  const llvm::Target *TheTarget = llvm::TargetRegistry::lookupTarget(MArch, TheTriple, Error);
  // Some modules don't specify a triple, and this is okay.
  if (!TheTarget) {
    return 0;
  }

  // Package up features to be passed to target/subtarget
  std::string FeaturesStr;
  if (MAttrs.size()) {
    llvm::SubtargetFeatures Features;
    for (unsigned i = 0; i != MAttrs.size(); ++i)
      Features.AddFeature(MAttrs[i]);
    FeaturesStr = Features.getString();
  }

  llvm::TargetOptions Options;
  return TheTarget->createTargetMachine(TheTriple.getTriple(),
                                        MCPU, FeaturesStr, Options,
                                        RelocModel, CMModel,
                                        llvm::CodeGenOpt::Aggressive);
}

void AbstractServer::optimizeModule(llvm::Module *Mod)
{
    llvm::PassManager AutoParVectPasses;
    TargetLibraryInfo *TLI = new llvm::TargetLibraryInfo(Triple(Mod->getTargetTriple()));
    AutoParVectPasses.add(TLI);
    AutoParVectPasses.add(new llvm::DataLayout(Mod->getDataLayout()));

    llvm::Triple ModuleTriple(Mod->getTargetTriple());
    llvm::TargetMachine *Machine = 0;
    if (ModuleTriple.getArch())
      Machine = GetTargetMachine(Triple(ModuleTriple));
    llvm::OwningPtr<llvm::TargetMachine> TM(Machine);

    // internal analysis passes from the target machine
    if (TM.get())
      TM->addAnalysisPasses(AutoParVectPasses);
    else
        std::cout << "INFO: " << "No TargetMachine detected\n";

    if (!DisablePolly) {
        // polly preparation passes
        AutoParVectPasses.add(llvm::createTypeBasedAliasAnalysisPass());
        AutoParVectPasses.add(llvm::createBasicAliasAnalysisPass());
        AutoParVectPasses.add(llvm::createPromoteMemoryToRegisterPass());
        AutoParVectPasses.add(llvm::createLoopSimplifyPass());
        AutoParVectPasses.add(polly::createIndVarSimplifyPass());

        // polly passes
        AutoParVectPasses.add(polly::createCodeGenerationPass());

        // cleanup passes
        AutoParVectPasses.add(llvm::createGlobalOptimizerPass());
        AutoParVectPasses.add(llvm::createCFGSimplificationPass());
        AutoParVectPasses.add(llvm::createInstructionCombiningPass());
    }

    if (!DisableVectorization) {
        // vectorization preparation passes
        AutoParVectPasses.add(llvm::createTypeBasedAliasAnalysisPass());
        AutoParVectPasses.add(llvm::createBasicAliasAnalysisPass());
        AutoParVectPasses.add(llvm::createPromoteMemoryToRegisterPass());
        AutoParVectPasses.add(llvm::createLoopSimplifyPass());

        // vectorization passes
        AutoParVectPasses.add(llvm::createLoopVectorizePass());
        AutoParVectPasses.add(llvm::createSLPVectorizerPass());
        //AutoParVectPasses.add(llvm::createBBVectorizePass());

        // cleanup passes
        AutoParVectPasses.add(llvm::createCFGSimplificationPass());
        AutoParVectPasses.add(llvm::createInstructionCombiningPass());
    }

    AutoParVectPasses.run(*Mod);
    llvm::verifyModule(*Mod, llvm::PrintMessageAction);

    if (DumpOptOut)
        Mod->dump();
}

llvm::GenericValue AbstractServer::handleCall(AbstractBackend* backend, char *marshalledCall, llvm::Function *&calledFunction, std::vector<llvm::GenericValue> &args, std::list<std::vector<llvm::GenericValue>::size_type> &indexesOfPointersInArgs)
{
    calledFunction = backend->parseMarshalledCallToFunction(marshalledCall);

    // use typeinfo from function object to unmarshal arguments
    
    #ifndef TIMING 
    auto StartTime = std::chrono::high_resolution_clock::now();
    #endif  
    unmarshalCallArgs(marshalledCall, calledFunction->getName().size(), calledFunction, args, indexesOfPointersInArgs);
    #ifndef TIMING 
    auto EndTime = std::chrono::high_resolution_clock::now();
    std::cout << "\n SERVR: MPI_DATA_TRANSFER C->S = " <<    std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime).count()  << "\n";
    #endif
        
    // finally, execute function call
    std::cout << "INFO" << ": running \"" << calledFunction->getName().str() << "\", having type '";
    std::cout.flush();
    calledFunction->getType()->dump();
    std::cout << "' in Engine\n";

    StartTime = std::chrono::high_resolution_clock::now();
    const llvm::GenericValue& ret = backend->callEngine(calledFunction, args);
    EndTime = std::chrono::high_resolution_clock::now();
    TimeDiffLastExecution = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
    #ifndef TIMING 
    std::cout << "\n SERVR: Execution Time = " <<    std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime).count() << "\n";
    #endif
    
    #ifndef NDEBUG
    std::cout << "DEBUG: " << "Function call returned, it took " << TimeDiffLastExecution.count() << " microseconds\n";
    #endif

    return ret;
}
