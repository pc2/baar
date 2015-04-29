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

#ifndef ABSTRACTBACKEND_H
#define ABSTRACTBACKEND_H

#include <list>
#include <unordered_map>
#include <functional>

#include "llvm/IR/Module.h"
#include "llvm/ExecutionEngine/GenericValue.h"

class AbstractBackend
{
public:
    AbstractBackend(llvm::Module *&Mod);
    virtual ~AbstractBackend();
    llvm::Function* parseMarshalledCallToFunction(const char *marshalledCall);
    virtual llvm::GenericValue callEngine(llvm::Function *F, const std::vector< llvm::GenericValue > &ArgValues) = 0;
private:
    static std::string parseFunctionName(const char *marshalledCall);
    std::unordered_map<std::string, llvm::Function *> calledFunction_map;
protected:
    std::unique_ptr<llvm::Module> Module;
};

#endif // ABSTRACTBACKEND_H
