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

#include "socketclient.h"

#include "polly/ScopDetection.h"
#include "polly/LinkAllPasses.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/BlockFrequencyInfo.h"
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/PassManager.h"
#include "llvm/Support/ValueHandle.h"
#include "llvm/Support/CommandLine.h"

#include "pass/rpcaccelerate.h"
#include "pass/accscore.h"
#include "abstractclient.h"
#include "shmemclient.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <list>
#include <unistd.h>
#include <string>
#include <cstring>
#include <unordered_map>
#include <sys/stat.h>
#include <signal.h>

static std::unique_ptr<AbstractClient> AccClient;
static std::unordered_map<std::string, unsigned long> scores;
static std::fstream timeMeasureStream;

llvm::cl::opt<int> OffloadScoreThreshold("offload-threshold", llvm::cl::desc("Functions getting a score greater than this are offloaded, defaults to zero"), llvm::cl::init(0));
llvm::cl::opt<int> ProgramRuns("runs", llvm::cl::desc("How often the acceleration on program and program itself should run, defaults to one. Reruns the whole process on program exit if greater one"), llvm::cl::init(1));
llvm::cl::opt<bool> DisableExeEngineParallel("disable-ee-parallel", llvm::cl::desc("Run Execution Engine sequentilly after analysis and alteration"), llvm::cl::init(false));

enum clientCommTypes {
    socket, sharedmem
};
llvm::cl::opt<clientCommTypes> ClientCommunicationType("comm", llvm::cl::desc("Choose type of communication the client uses:"), llvm::cl::init(socket),
                                              llvm::cl::values(clEnumVal(socket, "use communication over socket (default)"),
                                                               clEnumVal(sharedmem, "use communication over shared memory"),
                                                               clEnumValEnd));
llvm::cl::opt<std::string> ServerHostname("host", llvm::cl::desc("Server hostname or IP, defaults to 'localhost'"), llvm::cl::init("localhost"));
llvm::cl::opt<std::string> TimeMeasureFile("time-file", llvm::cl::desc("Output file for time measuring, defaults to 'time_measures.txt'"), llvm::cl::init("time_measures.txt"));

llvm::cl::opt<int> BBFreqThreshold("bbfreq-threshold", llvm::cl::desc("Basic block frequency threshold to consider functions for acceleration and analyze further"), llvm::cl::init(100));

llvm::cl::opt<std::string> IRFilename(llvm::cl::Positional, llvm::cl::Required, llvm::cl::desc("<IR file>"));
llvm::cl::list<std::string> InputArgv(llvm::cl::ConsumeAfter, llvm::cl::desc("<program arguments>..."));

static void printUsage(std::string programName);
static void runExeEngine(llvm::Module* Mod, llvm::ExecutionEngine* EE);
static void declareCallAcc(llvm::Module *ProgramMod);
static std::string exportFunctionsIntoBitcode(llvm::LLVMContext* Context, llvm::Module *ProgramMod, const std::list<llvm::Function*>& functionList);

static void handleSignal(int) {
    AccClient.reset(nullptr);
    timeMeasureStream << std::endl;
    timeMeasureStream.close();

    LLVMShutdown();

    exit(0);
}

extern "C" void callAcc(const char *retTypeFuncNameArgTypes, ...) {
    va_list args;
    va_start(args, retTypeFuncNameArgTypes);

    auto StartTime = std::chrono::high_resolution_clock::now();
    AccClient->callAcc(retTypeFuncNameArgTypes, args);
    auto EndTime = std::chrono::high_resolution_clock::now();

    auto funcNameStart = strchr(retTypeFuncNameArgTypes, ':');
    funcNameStart++;
    std::string funcName(funcNameStart, strcspn(funcNameStart, ":"));

    const auto TimeDiffCallAcc = EndTime - StartTime;
    timeMeasureStream << funcName << '\t' << scores[funcName] << '\t' <<
                         AccClient->getTimeDiffLastExecution() << '\t' << std::chrono::duration_cast<std::chrono::microseconds>(TimeDiffCallAcc).count() << '\t';
    timeMeasureStream.flush();
    std::cout << "INFO: callAcc took " << std::chrono::duration_cast<std::chrono::microseconds>(TimeDiffCallAcc).count() << " microseconds for '" << funcName << "' with score " << scores[funcName] << "\n";

    va_end(args);
}

int main(int argc, char* argv[]) {
    llvm::cl::ParseCommandLineOptions(argc, argv);

    struct sigaction signal_action;
    memset( &signal_action, 0, sizeof(signal_action) );
    signal_action.sa_handler = &handleSignal;
    sigfillset(&signal_action.sa_mask);
    sigaction(SIGINT,&signal_action,NULL);

    struct stat buf;
    bool fileExists = stat(TimeMeasureFile.c_str(), &buf) != -1;
    timeMeasureStream.open(TimeMeasureFile, std::ios::out | std::ios::app);
    if (!fileExists)
        timeMeasureStream << "Ana\tSrvIni\tOpt\tBndIni\tAlt\t(FuncName\tFuncScore\tExeAcc\tCallAcc)*\n";

    for (int i = 0; i < ProgramRuns; i++) {
        switch(ClientCommunicationType) {
            case socket: AccClient.reset(new SocketClient(ServerHostname)); break;
            case sharedmem:
                AccClient.reset(new ShmemClient());
                if (ProgramRuns > 1) {
                    std::cout << "WARNING: -runs argument unsupported in shared memory mode, will be ignored\n";
                    ProgramRuns = 1;
                }
            break;
        }
        scores.clear();
        if (ProgramRuns > 1)
            std::cout << "INFO: " << "---------- " << "Beginning run " << i+1 << " of " << ProgramRuns << " ----------" << std::endl;

        // prepare LLVM ExecutionEngine
        llvm::InitializeNativeTarget();
        llvm::LLVMContext* Context = &llvm::getGlobalContext();

        // parse program IR from file
        llvm::SMDiagnostic Err;
        llvm::Module *ProgramMod = llvm::ParseIRFile(IRFilename, Err, *Context);
        if (!ProgramMod) {
            Err.print("acc_client", llvm::errs());
            exit(1);
        }
        llvm::verifyModule(*ProgramMod, llvm::PrintMessageAction);

        // create ExecutionEngine
        std::string err;
        std::unique_ptr<llvm::ExecutionEngine> ExeEngine(llvm::ExecutionEngine::create(ProgramMod, false, &err));
        if (!err.empty()) {
            std::cerr << "ERROR, " << err << std::endl;
            exit(1);
        }
        // generate code for main before starting parallel thread to avoid segfaults
        ExeEngine->getPointerToFunction(ProgramMod->getFunction("main"));

        std::unique_ptr<std::thread> exeEngineThread;
        if (!DisableExeEngineParallel) {
            // start execution of program in parallel thread
            exeEngineThread.reset(new std::thread(runExeEngine, ProgramMod, ExeEngine.get()));
        }

        auto StartTimeAnalysis = std::chrono::high_resolution_clock::now();
        // ------ Analyze functions to gather candidates for acceleration
        // Passes for estimating Basic Block frequency
        llvm::FunctionPassManager BlockFreqAnalysisPM(ProgramMod);
        BlockFreqAnalysisPM.add(llvm::createLoopSimplifyPass());
        llvm::BlockFrequencyInfo* BFIPass = new llvm::BlockFrequencyInfo();
        BlockFreqAnalysisPM.add(BFIPass);

        std::list<llvm::Function*> accelerationCandidates;
        for (llvm::Module::iterator I = ProgramMod->begin(); I != ProgramMod->end(); I++) {
            if (I->getName().compare("main") == 0) // do not try to accelerate main
                continue;

            BlockFreqAnalysisPM.run(*I);

            unsigned maxBBFreq = 0;
            for (llvm::Function::iterator J = I->begin(); J != I->end(); J++) {
                auto currFreq = BFIPass->getBlockFreq(J).getFrequency()/BFIPass->getBlockFreq(J).getEntryFrequency();
                if (maxBBFreq < currFreq)
                    maxBBFreq = currFreq;
            }
            #ifndef NDEBUG
                    std::cout << "DEBUG: " << I->getName().str() << " max BB freq = " << maxBBFreq << std::endl;
            #endif
            if (maxBBFreq > BBFreqThreshold)
                accelerationCandidates.push_back(&(*I));
        }
        std::cout << "INFO: " << "The following function" << (accelerationCandidates.size() > 1 ? "s were" : " was") << " chosen as acceleration candidate" << (accelerationCandidates.size() > 1 ? "s: " : ": ");
        for (const auto& func : accelerationCandidates)
            std::cout << func->getName().str() << " ";
        std::cout << std::endl;

        // Score and choose functions to accelerate from candidates
        // Passes for scoring functions
        llvm::FunctionPassManager ScoringPM(ProgramMod);
        ScoringPM.add(new llvm::DataLayout(ProgramMod->getDataLayout()));
        ScoringPM.add(llvm::createPromoteMemoryToRegisterPass());
        // ScoringPM.add(llvm::createLoopSimplifyPass()); alredy done by BlockFreqAnalysisPM, but needed here!
        ScoringPM.add(polly::createIndVarSimplifyPass());
        ScoringPM.add(llvm::createBasicAliasAnalysisPass());
        ScoringPM.add(new polly::ScopDetection());
        baar::AccScore* ScorePass = static_cast<baar::AccScore*>(baar::createScoringPass());
        ScoringPM.add(ScorePass);

        std::list<llvm::Function*> functionsToAccelerate;
        for (const auto& function : accelerationCandidates) {
            ScoringPM.run(*function);
            if (ScorePass->getScore() > OffloadScoreThreshold) { // exclude functions obviously promising no speedup on accelerator
                functionsToAccelerate.push_back(function);
                scores[function->getName().str()] = ScorePass->getScore();
            }
        }
        accelerationCandidates.clear();
        auto EndTimeAnalysis = std::chrono::high_resolution_clock::now();
        const auto TimeDiffAnalysis = EndTimeAnalysis - StartTimeAnalysis;
        timeMeasureStream << std::chrono::duration_cast<std::chrono::microseconds>(TimeDiffAnalysis).count() << '\t';
        std::cout << "INFO: Analysis took " << std::chrono::duration_cast<std::chrono::microseconds>(TimeDiffAnalysis).count() << " microseconds\n";
        // functionsToAccelerate.push_back(ProgramMod->getFunction("testfunc")); // ----- DEBUG ----- // TODO add command line argument to always accelerate a desired function (?)
        std::cout << "INFO: " << "The following function" << (functionsToAccelerate.size() > 1 ? "s were" : " was") << " chosen to be accelerated: ";
        for (const auto& func : functionsToAccelerate)
            std::cout << func->getName().str() << " ";
        std::cout << std::endl;

        auto StartTimeAccInitialization = std::chrono::high_resolution_clock::now();
        // ------ prepare the acceleration by declaring callAcc and creating module with exported functions
        declareCallAcc(ProgramMod);
        std::string exportedModuleIR = exportFunctionsIntoBitcode(Context, ProgramMod, functionsToAccelerate);
        // initialise client (connect to server and send exported functions)
        std::cout << "DEBUG: " << "export done, initialising accelerator...\n";
        AccClient->initialiseAccelerationWithIR(exportedModuleIR);
        auto EndTimeAccInitialization = std::chrono::high_resolution_clock::now();
        const auto TimeDiffAccInitialization = EndTimeAccInitialization - StartTimeAccInitialization;
        timeMeasureStream << std::chrono::duration_cast<std::chrono::microseconds>(TimeDiffAccInitialization).count() << '\t';
        std::cout << "INFO: Initializing the server took " << std::chrono::duration_cast<std::chrono::microseconds>(TimeDiffAccInitialization).count() << " microseconds\n";
        timeMeasureStream << AccClient->getTimeDiffOpt() << '\t' << AccClient->getTimeDiffInit() << '\t';

        // ------ do the actual acceleration by running RPCAcc pass on every function to be accelerated
        auto StartTimeAlteration = std::chrono::high_resolution_clock::now();
        bool success = true;
        llvm::FunctionPassManager FuncToRPCPM(ProgramMod);
        FuncToRPCPM.add(baar::createRPCAcceleratePass(&scores));
        for (const auto& function : functionsToAccelerate) {
            success &= FuncToRPCPM.run(*function);
            ExeEngine->recompileAndRelinkFunction(function);
        }
        auto EndTimeAlteration = std::chrono::high_resolution_clock::now();
        const auto TimeDiffAlteration = EndTimeAlteration - StartTimeAlteration;
        timeMeasureStream << std::chrono::duration_cast<std::chrono::microseconds>(TimeDiffAlteration).count() << '\t';
        std::cout << "INFO: Altering the local program to use accelerator took " << std::chrono::duration_cast<std::chrono::microseconds>(TimeDiffAlteration).count() << " microseconds\n";

        if (!DisableExeEngineParallel)
            exeEngineThread->join();
        else
            runExeEngine(ProgramMod, ExeEngine.get());
        timeMeasureStream << std::endl;
    }
    timeMeasureStream.close();
    
    LLVMShutdown();
}

void runExeEngine(llvm::Module* Mod, llvm::ExecutionEngine* EE) {
    std::cout << "INFO: " << "starting module main\n";
    // insert module name in front of argv
    InputArgv.insert(InputArgv.begin(), IRFilename);
    // run program main
    EE->runFunctionAsMain(Mod->getFunction("main"), InputArgv, nullptr);
}

void printUsage(std::string programName) {
    std::cout << "\nUsage:" << std::endl;
    std::cout << programName << " --socket <program in LLVM IR>\t\t - \t Communicate over sockets" << std::endl;
    std::cout << programName << " --sharedmem <program in LLVM IR>\t - \t Communicate over shared memory" << std::endl << std::endl;
}

void declareCallAcc(llvm::Module *ProgramMod) {
    // Create FunctionType for callAcc, externally defined
    std::vector<llvm::Type*>CallAccType_args;
    llvm::PointerType* CallAccType_arg1Ty = llvm::PointerType::get(llvm::IntegerType::get(ProgramMod->getContext(), 8), 0);	 // char *
    CallAccType_args.push_back(CallAccType_arg1Ty);
    llvm::FunctionType *CallAccType = llvm::FunctionType::get(llvm::Type::getVoidTy(ProgramMod->getContext()), CallAccType_args, true);

    // Create prototype for callAcc
    llvm::Function::Create(CallAccType, llvm::GlobalValue::ExternalLinkage, "callAcc", ProgramMod);
}

std::string exportFunctionsIntoBitcode(llvm::LLVMContext* Context, llvm::Module *ProgramMod, const std::list<llvm::Function*>& functionList) { // See LLVM CloneModule.cpp
    // TODO optimize such that only used globals are copied

    // Map values from the old to values in the new module
    llvm::ValueToValueMapTy VMap;

    // Create new Module to export functions into
    llvm::Module* ExportModule = new llvm::Module("export", *Context);
    ExportModule->setDataLayout(ProgramMod->getDataLayout());
    ExportModule->setTargetTriple(ProgramMod->getTargetTriple());
    ExportModule->setModuleInlineAsm(ProgramMod->getModuleInlineAsm());

    // Copy global variables over to the new module, initializers are copied later
    for (llvm::Module::const_global_iterator I = ProgramMod->global_begin(), E = ProgramMod->global_end(); I != E; ++I) {
      llvm::GlobalVariable *GV = new llvm::GlobalVariable(*ExportModule,
                                              I->getType()->getElementType(),
                                              I->isConstant(), I->getLinkage(),
                                              (llvm::Constant*) 0, I->getName(),
                                              (llvm::GlobalVariable*) 0,
                                              I->getThreadLocalMode(),
                                              I->getType()->getAddressSpace());
      GV->copyAttributesFrom(I);
      VMap[I] = GV;
    }

    // Copy external function declarations for defined functions to the new module
    for (llvm::Module::const_iterator I = ProgramMod->begin(), E = ProgramMod->end(); I != E; ++I) {
      if (I->getName().str() == "main") // do not declare main, may cause problems with external compiler backend
          continue;
      if (I->isDeclaration() && (!I->getName().count("alloc") && !I->getName().count("free"))) // exclude declaration but include malloc etc. (safety measure for extcompiler backend)
          continue;
      llvm::Function *NF = llvm::Function::Create(llvm::cast<llvm::FunctionType>(I->getType()->getElementType()), I->getLinkage(), I->getName(), ExportModule);
      NF->copyAttributesFrom(I);
      VMap[I] = NF;
    }

    // Copy aliases to the new module
    for (llvm::Module::const_alias_iterator I = ProgramMod->alias_begin(), E = ProgramMod->alias_end(); I != E; ++I) {
      llvm::GlobalAlias *GA = new llvm::GlobalAlias(I->getType(), I->getLinkage(), I->getName(), NULL, ExportModule);
      GA->copyAttributesFrom(I);
      VMap[I] = GA;
    }

    // Any dependencies initializers might have, are copied now. Copy and set initilializers
    for (llvm::Module::const_global_iterator I = ProgramMod->global_begin(), E = ProgramMod->global_end(); I != E; ++I) {
      llvm::GlobalVariable *GV = llvm::cast<llvm::GlobalVariable>(VMap[I]);
      if (I->hasInitializer())
        GV->setInitializer(llvm::MapValue(I->getInitializer(), VMap));
    }

    // Copy chosen functions into newly created module
    for (const auto& ToBeAccelerated : functionList) {
        // Get reference to function which we want to accelerate
        llvm::Function *ExportFunction = llvm::cast<llvm::Function>(VMap[ToBeAccelerated]);

        llvm::Function::arg_iterator ExpI = ExportFunction->arg_begin();
              for (llvm::Function::const_arg_iterator ToBeAccI = ToBeAccelerated->arg_begin(); ToBeAccI != ToBeAccelerated->arg_end(); ++ToBeAccI) {
                ExpI->setName(ToBeAccI->getName());
                VMap[ToBeAccI] = ExpI++;
              }

        llvm::SmallVector<llvm::ReturnInst*, 8> Returns;  // Ignore returns cloned.
        llvm::CloneFunctionInto(ExportFunction, ToBeAccelerated, VMap, /*ModuleLevelChanges=*/true, Returns);
    }

    // Copy missing aliases
    for (llvm::Module::const_alias_iterator I = ProgramMod->alias_begin(), E = ProgramMod->alias_end(); I != E; ++I) {
      llvm::GlobalAlias *GA = llvm::cast<llvm::GlobalAlias>(VMap[I]);
      if (const llvm::Constant *C = I->getAliasee())
        GA->setAliasee(llvm::MapValue(C, VMap));
    }

    // Copy metadata
    for (llvm::Module::const_named_metadata_iterator I = ProgramMod->named_metadata_begin(), E = ProgramMod->named_metadata_end(); I != E; ++I) {
      const llvm::NamedMDNode &NMD = *I;
      llvm::NamedMDNode *NewNMD = ExportModule->getOrInsertNamedMetadata(NMD.getName());
      for (unsigned i = 0, e = NMD.getNumOperands(); i != e; ++i)
        NewNMD->addOperand(MapValue(NMD.getOperand(i), VMap));
    }

    // Write ExportModule to IR file
    // std::string err;
    // llvm::raw_fd_ostream moduleStream(fileName.c_str(), err); // for writing to file (deprecated)
    std::string moduleBitcode;
    llvm::raw_string_ostream moduleStream(moduleBitcode);
    //llvm::WriteBitcodeToFile(ExportModule, moduleStream); // human unreadable bitcode TODO not working correctly
    moduleStream << *ExportModule; // human readable IR
    // moduleStream.close();
    moduleStream.flush();
    return moduleBitcode;
}
