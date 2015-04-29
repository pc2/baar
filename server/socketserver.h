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

#ifndef SOCKETSERVER_H
#define SOCKETSERVER_H

#include "mpi.h"

#include "abstractserver.h"
#include "llvm/IR/DerivedTypes.h"
#include "../common/mpihelper.h"

class SocketServer : public AbstractServer
{
public:
    SocketServer(backendTypes backendType);
    virtual ~SocketServer();

protected:
    virtual void initCommunication();
    virtual void handleCommunication();
    virtual void cleanupCommunication();
    virtual void unmarshalCallArgs(char *buffer, int functionName_offset, llvm::Function *calledFunction, std::vector<llvm::GenericValue> &args, std::list<std::vector<llvm::GenericValue>::size_type> &indexesOfPointersInArgs);

private:
    int sockfd;

    // MPI_CONNECTION_INIT
    MPI_Comm client; 
    char port_name[MPI_MAX_PORT_NAME];

    void handle_conn(int sockfd);
    struct ArgumentList argumentList[MAX_NUMBER_OF_ARGUMENTS];
    MPI_Datatype ArgListType;
};

#endif // SOCKETSERVER_H
