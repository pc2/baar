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

#ifndef ABSTRACTSERVER_H
#define ABSTRACTSERVER_H

#include "../consts.h"

#include <unordered_map>
#include <vector>
#include <list>
#include <chrono>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/PassManager.h"

#include "abstractbackend.h"

class AbstractServer
{
public:
    enum backendTypes {
        extcompiler, jit, interpret
    };

    AbstractServer(backendTypes backendType);
    virtual ~AbstractServer();
    void start();
    void end();


protected:
    static inline void error(const char *msg)
    {
        perror(msg);
        exit(1);
    }
    virtual void initCommunication() = 0;
    virtual void handleCommunication() = 0;
    virtual void cleanupCommunication() = 0;
    virtual void unmarshalCallArgs(char *buffer, int functionName_offset, llvm::Function *calledFunction, std::vector<llvm::GenericValue> &args, std::list<std::vector<llvm::GenericValue>::size_type> &indexesOfPointersInArgs) = 0;

    std::unique_ptr<AbstractBackend> parseIRtoBackend(const char *ir_buffer);
    void optimizeModule(llvm::Module* Mod);
    llvm::GenericValue handleCall(AbstractBackend* backend, char *marshalledCall, llvm::Function *&calledFunction, std::vector<llvm::GenericValue> &args, std::list<std::vector<llvm::GenericValue>::size_type> &indexesOfPointersInArgs);

    backendTypes backendType;
    std::chrono::microseconds TimeDiffOpt;
    std::chrono::microseconds TimeDiffInit;
    std::chrono::microseconds TimeDiffLastExecution;
private:
    llvm::TargetMachine *GetTargetMachine(llvm::Triple TheTriple);
};

#endif // ABSTRACTSERVER_H
