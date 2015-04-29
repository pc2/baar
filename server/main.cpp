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

#include "mpi.h"

#include "abstractserver.h"
#include "shmemserver.h"
#include "socketserver.h"

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/LinkAllPasses.h"

#include "polly/RegisterPasses.h"
#include "polly/LinkAllPasses.h"

#include <signal.h>

#include <iostream>

using namespace std;

enum srvCommTypes {
    socket, sharedmem
};
llvm::cl::opt<srvCommTypes> ServerCommunicationType("comm", llvm::cl::desc("Choose type of communication the server uses:"), llvm::cl::init(socket),
                                              llvm::cl::values(clEnumVal(socket, "use communication over socket (default)"),
                                                               clEnumVal(sharedmem, "use communication over shared memory"),
                                                               clEnumValEnd));

llvm::cl::opt<AbstractServer::backendTypes> backendType("backend", llvm::cl::desc("Choose type of backend the server uses:"), llvm::cl::init(AbstractServer::jit),
                                              llvm::cl::values(clEnumValN(AbstractServer::extcompiler, "extcompiler", "generate C(++) code from accepted IR and compile with external compiler"),
                                                               clEnumValN(AbstractServer::jit, "jit", "use just-in-time compilation (default)"),
                                                               clEnumValN(AbstractServer::interpret, "interpret", "interpret IR (slow)"),
                                                               clEnumValEnd));

static std::unique_ptr<AbstractServer> server(nullptr);

void handleSignal(int)
{
    server->end();
    exit(0);
}

int main(int argc, char* argv[])
{
    llvm::PassRegistry &Registry = *llvm::PassRegistry::getPassRegistry();
    // initializePollyPasses(Registry); TODO use in LLVM 3.5+
    initializeCore(Registry);
    initializeDebugIRPass(Registry);
    initializeScalarOpts(Registry);
    initializeObjCARCOpts(Registry);
    initializeVectorization(Registry);
    initializeIPO(Registry);
    initializeAnalysis(Registry);
    initializeIPA(Registry);
    initializeTransformUtils(Registry);
    initializeInstCombine(Registry);
    initializeInstrumentation(Registry);
    initializeTarget(Registry);

    llvm::cl::ParseCommandLineOptions(argc, argv);

    struct sigaction signal_action;
    memset( &signal_action, 0, sizeof(signal_action) );
    signal_action.sa_handler = &handleSignal;
    sigfillset(&signal_action.sa_mask);
    sigaction(SIGINT,&signal_action,NULL);

    switch (ServerCommunicationType) {
        case socket: server.reset(new SocketServer(backendType)); break;
        case sharedmem: server.reset(new ShmemServer(backendType)); break;
    }

    if (server)
        server->start();

    server->end();
    
    return 0;
}
