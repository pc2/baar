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

#include "jitbackend.h"

#include "llvm/Support/TargetSelect.h"

#include <iostream>

JITBackend::JITBackend(llvm::Module *&Mod) : AbstractBackend(Mod)
{
    // prepare LLVM ExecutionEngine
    llvm::InitializeNativeTarget();

    std::string err;
    executionEngine.reset(llvm::ExecutionEngine::create(Module.get(), false, &err));

    if (!err.empty()) {
        std::cerr << "ERROR, " << err << std::endl;
        exit(1);
    }

    executionEngine->generateCodeForModule(Module.get());
}

JITBackend::~JITBackend()
{
    Module.release(); // executionEngine takes care of freeing the Module
}

llvm::GenericValue JITBackend::callEngine(llvm::Function *F, const std::vector<llvm::GenericValue> &ArgValues)
{
    return executionEngine->runFunction(F, ArgValues);
}
