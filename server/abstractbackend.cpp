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

#include "abstractbackend.h"

#include <iostream>
#include <functional>

AbstractBackend::AbstractBackend(llvm::Module*& Mod) : Module(Mod)
{

}

AbstractBackend::~AbstractBackend()
{

}

llvm::Function *AbstractBackend::parseMarshalledCallToFunction(const char *marshalledCall)
{
    std::string calledFunctionName = parseFunctionName(marshalledCall);
#ifndef NDEBUG
   std::cout << "DEBUG" << ": interpreting as call '" << calledFunctionName << "(";
#endif
    // get the function object
    llvm::Function* calledFunction;
    auto calledFunction_mapResult = calledFunction_map.find(calledFunctionName);
    if (calledFunction_mapResult == calledFunction_map.end()) { // if called function not in map, get pointer + insert
        calledFunction = Module->getFunction(calledFunctionName);
        calledFunction_map[calledFunctionName] = calledFunction;
    } else	// if called function in map, use pointer from map
        calledFunction = calledFunction_mapResult->second;

    assert(calledFunction != nullptr);

    return calledFunction;
}

std::string AbstractBackend::parseFunctionName(const char *marshalledCall)
{
    char calledFunctionName[128];
    char *calledFunctionName_ptr = &calledFunctionName[0];
    const char *getCallName_ptr = marshalledCall;
    for (; *getCallName_ptr != '\0' && *getCallName_ptr != ':'; getCallName_ptr++, calledFunctionName_ptr++)
        *calledFunctionName_ptr = *getCallName_ptr;
    *calledFunctionName_ptr = '\0';

    return std::string(calledFunctionName);
}
