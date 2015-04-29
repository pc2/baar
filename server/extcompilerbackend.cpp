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

#include "extcompilerbackend.h"
#include "llvm_ffi.cpp"

#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>
#include <cstdio>

#include <iostream>
#include <string>

ExtCompilerBackend::ExtCompilerBackend(llvm::Module *&Mod) : AbstractBackend(Mod), export_name(std::to_string(getpid()) + "_module_export")
{
    call_remote_compiler_sh = buildShellScript(export_name);

    extern bool WriteCXXFile(llvm::Module *module, const char *fn, int vectorWidth, const char *includeName);
#ifdef _K1OM_
    WriteCXXFile(Mod, std::string(export_name + ".cpp").c_str(), 8, "knc-i1x8.h");
#else
    WriteCXXFile(Mod, std::string(export_name + ".cpp").c_str(), 16, "generic-16.h");
#endif

    signal(SIGCHLD, SIG_DFL);
    if (system(call_remote_compiler_sh.c_str())) {
        std::cout << "ERROR while calling remote compiler. \n";
        exit(-1);
    }
    signal(SIGCHLD, SIG_IGN);

    if (export_library = dlopen(std::string("./" + export_name + ".so").c_str(), RTLD_NOW  | RTLD_GLOBAL))
        std::cout << "INFO: " << export_name << ".so" << " successfully loaded." << std::endl;
    else
        std::cout << "ERROR loading " << dlerror() << std::endl;
}

ExtCompilerBackend::~ExtCompilerBackend() {
    if (export_library)
        dlclose(export_library);
    remove(std::string(export_name + ".cpp").c_str());
    remove(std::string(export_name + ".so").c_str());
}

llvm::GenericValue ExtCompilerBackend::callEngine(llvm::Function *F, const std::vector<llvm::GenericValue> &ArgValues) {
    RawFunc F_ptr;
    if (*(void**)(&F_ptr) = dlsym(export_library, F->getName().str().c_str()))
        std::cout << "INFO: " << "Found " << F->getName().str() << " in library." << std::endl;
    else
        std::cout << "ERROR: " << "Could not find " << F->getName().str() << " in library." << std::endl;

    llvm::GenericValue ret;
    ffiInvoke(F_ptr, F, ArgValues, new llvm::DataLayout(Module.get()), ret); // TODO DataLayout is not initialized for target

    return ret;
}

std::string ExtCompilerBackend::buildShellScript(const std::string &export_name)
{
#ifdef _K1OM_
    return std::string("#!/bin/sh\n"
                       "remote_host=${SSH_CONNECTION%%' '*} \n"
                       "scp "+export_name+".cpp $remote_host:. \n"
                       "ssh $remote_host 'module add intel/compiler gcc/4.8.1 cmake/2.8.10.2 && "
                       "icc -w -mmic -o "+export_name+".so -shared -fPIC "+export_name+".cpp && "
                       "scp "+export_name+".so '\"$(hostname):.\"' \n'");
#else
    return std::string("#!/bin/sh\n"
                       "clang++ -w -o "+export_name+".so -shared -fPIC "+export_name+".cpp \n");
#endif
}
