//    Copyright (c) 2014 Marvin Damschen (marvin.damschen@gullz.de)

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

#include "socketserver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cinttypes>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <limits.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>

#include "llvm/IR/Function.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/APFloat.h"

#include "../common/sockethelperfunctions.h"

SocketServer::SocketServer(backendTypes backendType) : AbstractServer(backendType)
{
}

void SocketServer::initCommunication()
{
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // create IP, Stream (-> TCP) socket
    if (sockfd < 0)
        error("ERROR, could not open socket");

    struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr)); // initialise server_addr (by setting it all to zero)
    server_addr.sin_family = AF_INET; // this is an IP address
    server_addr.sin_addr.s_addr = INADDR_ANY; // set server's IP address to this machine's IP address
    server_addr.sin_port = htons(SERVER_PORT); // set server's port with correct byte order

    if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        error("ERROR, could not bind socket to server address");

    listen(sockfd, 5); // start listening for connections
    std::cout << "INFO" << ": waiting for connections" << std::endl;
}

void SocketServer::handleCommunication()
{
    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);
    signal(SIGCHLD, SIG_IGN); // We are not interested in results from the children
    while (1) {
        // get next incoming connection, block if none
        int newsockfd = accept(sockfd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (newsockfd < 0)
            error("ERROR, could not retrieve incoming connection");

        int pid = fork(); // new process for every connection
        if (pid < 0)
            error("ERROR, could not fork process");
        if (pid == 0) { // child
            close(sockfd);
            std::cout << "New client connection assigned to process " << getpid() << std::endl;
            handle_conn(newsockfd);
            exit(0);
        } else // parent
            close(newsockfd);
    }
}

void SocketServer::cleanupCommunication()
{
    close(sockfd);
}

void SocketServer::unmarshalCallArgs(char *buffer, int functionName_offset, llvm::Function *calledFunction, std::vector<llvm::GenericValue> &args, std::list<std::vector<llvm::GenericValue>::size_type> &indexesOfPointersInArgs)
{
    llvm::FunctionType *CalledFuncType = calledFunction->getFunctionType();
    int numArgs = CalledFuncType->getNumParams();

    // skip function name in msg from client
    buffer+=functionName_offset;

    // parse arguments into GenericValues for the ExecutionEngine
    for (int i = 0; i < numArgs; i++) {
        llvm::GenericValue CurrArg;
        llvm::Type* CurrType = CalledFuncType->getParamType(i);
        llvm::Type* pointerToTy;
        buffer++;
        switch (CurrType->getTypeID()) {
        case llvm::Type::PointerTyID:
                pointerToTy = static_cast<llvm::PointerType*>(CurrType)->getElementType();
                while (pointerToTy->getTypeID() == llvm::Type::ArrayTyID || pointerToTy->getTypeID() == llvm::Type::PointerTyID)
                    pointerToTy = llvm::cast<llvm::SequentialType>(pointerToTy)->getElementType();
                CurrArg.PointerVal = SocketHelperFunctions::unmarshalArrayFromBufferAsTypeIntoNewMemory(buffer, pointerToTy);

                indexesOfPointersInArgs.push_back(i);
            break;
        case llvm::Type::FloatTyID:
            CurrArg.FloatVal = (float) strtod(buffer, &buffer);
            break;
        case llvm::Type::DoubleTyID:
            CurrArg.DoubleVal = strtod(buffer, &buffer);
            break;
        case llvm::Type::X86_FP80TyID: {
            size_t valstring_size = strcspn(buffer, ":\0");
            std::string valstring(buffer, valstring_size);
            buffer += valstring_size;
            llvm::APFloat bigval(llvm::APFloat::x87DoubleExtended, valstring);
            CurrArg.IntVal = bigval.bitcastToAPInt();
            break;
        }
        case llvm::Type::FP128TyID: {
            size_t valstring_size = strcspn(buffer, ":\0");
            std::string valstring(buffer, valstring_size);
            buffer += valstring_size;
            llvm::APFloat bigval(llvm::APFloat::IEEEquad, valstring);
            CurrArg.IntVal = bigval.bitcastToAPInt();
            break;
        }
        case llvm::Type::IntegerTyID: { // Note: LLVM does not differentiate between signed/unsiged int types
            uint64_t intval = strtoull(buffer, &buffer, 16);
            llvm::APInt apintval(((llvm::IntegerType*)CurrType)->getBitWidth(), intval);
            CurrArg.IntVal = apintval;
            break;
        }
        default:
            std::cerr << "LLVM TypeID " << CurrType->getTypeID() << " of argument " << i+1 << " is not supported.\n";
            exit(1);
        }
        args.push_back(CurrArg);

#ifndef NDEBUG
        switch (CurrType->getTypeID()) {
        case llvm::Type::PointerTyID:
            std::cout << "ptr to TyID " << pointerToTy->getTypeID();
            break;
        case llvm::Type::FloatTyID:
            std::cout << CurrArg.FloatVal;
            break;
        case llvm::Type::DoubleTyID:
            std::cout << CurrArg.DoubleVal;
            break;
        case llvm::Type::IntegerTyID:
            std::cout << *(CurrArg.IntVal.getRawData());
            break;
        case llvm::Type::X86_FP80TyID:
        case llvm::Type::FP128TyID:
            std::cout << "(long double)" << CurrArg.IntVal.toString(10U, false);
            break;
        default: std::cout << "TODO";
        }
        std::cout << ((i == numArgs-1)? ")'.\nDEBUG: Note, LLVM represents all ints as uint64_t, long double as two uint64_t internally.\n" : ", ");
#endif
    }
    if (numArgs == 0)
        std::cout << ")'.\n";
}

void SocketServer::handle_conn(int sockfd)
{
    char *module_ir_buffer = (char *) calloc(IR_BUFFER_SIZE, sizeof(char));
    char *module_ir_buffer_free_ptr = module_ir_buffer;
    unsigned long ir_length;
    int num_chars = recv(sockfd, module_ir_buffer, std::to_string(ULONG_MAX).length(), 0);
    if (num_chars == 0) {
        std::cout << "Client assigned to process " << getpid() << " has closed its socket \n";
        exit(0);
    }
    ir_length = strtoul(module_ir_buffer, &module_ir_buffer, 10);

    num_chars = recv(sockfd, module_ir_buffer + strlen(module_ir_buffer), ir_length - strlen(module_ir_buffer), MSG_WAITALL);
    if (num_chars == 0) {
        std::cout << "Client assigned to process " << getpid() << " has closed its socket \n";
        exit(0);
    }
    if (num_chars < 0)
        error("ERROR, could not read from socket");

    auto backend = parseIRtoBackend(module_ir_buffer);
    // notify client that calls can be accepted now by sending time taken for optimizing module and initialising backend
    const std::string readyStr(std::to_string(TimeDiffOpt.count()) + ":" + std::to_string(TimeDiffInit.count()));
    num_chars = send(sockfd, readyStr.c_str(), readyStr.size()+1, 0);
    if (num_chars < 0)
        error("ERROR, could not write to socket");
    free(module_ir_buffer_free_ptr);

    // initialise msg_buffer
    std::shared_ptr<char> msg_buffer((char*)calloc(MSG_BUFFER_SIZE, sizeof(char)), &free);
    while (1) {
        bzero(msg_buffer.get(), MSG_BUFFER_SIZE);
        // first acquire message length
        unsigned msg_length;
        auto UINT_MAX_str_len = std::to_string(UINT_MAX).length();
        num_chars = recv(sockfd, msg_buffer.get(), UINT_MAX_str_len + 1, 0);
        if (num_chars == 0) {
            std::cout << "Client assigned to process " << getpid() << " has closed its socket \n";
            exit(0);
        }
        char* msg_buffer_after_length_ptr;
        msg_length = strtoul(msg_buffer.get(), &msg_buffer_after_length_ptr, 10);
        // then get the message, some parts might have come with the length previously
        auto msg_part_length = strlen(msg_buffer.get());
        if (num_chars < msg_length)
            num_chars = recv(sockfd, msg_buffer.get() + msg_part_length, msg_length - msg_part_length, MSG_WAITALL);
        if (num_chars == 0) {
            std::cout << "Client assigned to process " << getpid() << " has closed its socket \n";

            backend.reset();
            exit(0);
        }
        if (num_chars < 0)
            error("ERROR, could not read from socket");

#ifndef NDEBUG
        //std::cout << getpid() << ": got message \"" << msg_buffer << "\"\n"; // TODO command line argument to print messages
        std::cout << getpid() << ": got message \n";
#endif
        llvm::Function* calledFunction = nullptr;
        std::vector<llvm::GenericValue> args;
        std::list<std::vector<llvm::GenericValue>::size_type> indexesOfPointersInArgs;
        llvm::GenericValue result = handleCall(backend.get(), msg_buffer_after_length_ptr, calledFunction, args, indexesOfPointersInArgs);

        // reset buffer and write time taken to buffer
        bzero(msg_buffer.get(), MSG_BUFFER_SIZE);
        sprintf(msg_buffer.get(), ";%ld", (long)TimeDiffLastExecution.count());

        // write changes to args and result back to buffer
        for (const auto& indexOfPtr : indexesOfPointersInArgs) {
            auto paramType = calledFunction->getFunctionType()->getParamType(indexOfPtr);
            while (paramType->getTypeID() == llvm::Type::ArrayTyID || paramType->getTypeID() == llvm::Type::PointerTyID)
                paramType = llvm::cast<llvm::SequentialType>(paramType)->getElementType();

            if (paramType->getTypeID() == llvm::Type::IntegerTyID) {
                SocketHelperFunctions::marshallArrayOfSizeAndTypeIntoBuffer(args[indexOfPtr].PointerVal, args[indexOfPtr+1].IntVal.getSExtValue(), std::pair<llvm::Type::TypeID, unsigned>(paramType->getTypeID(), ((llvm::IntegerType*)paramType)->getBitWidth()), msg_buffer.get());
            } else
                SocketHelperFunctions::marshallArrayOfSizeAndTypeIntoBuffer(args[indexOfPtr].PointerVal, args[indexOfPtr+1].IntVal.getSExtValue(), std::pair<llvm::Type::TypeID, unsigned>(paramType->getTypeID(), 0U), msg_buffer.get());

            free(args[indexOfPtr].PointerVal);
        }

        char returnValStr[MAX_VAL_SIZE];
        switch (calledFunction->getReturnType()->getTypeID()) {
            case llvm::Type::VoidTyID:
                sprintf(returnValStr, ":");
                break;
            case llvm::Type::FloatTyID:
                sprintf(returnValStr, ":%a", result.FloatVal);
                break;
            case llvm::Type::DoubleTyID:
                sprintf(returnValStr, ":%la", result.DoubleVal);
                break;
            case llvm::Type::X86_FP80TyID:
                returnValStr[0]=':';
                llvm::APFloat(llvm::APFloat::x87DoubleExtended, result.IntVal).convertToHexString(returnValStr+1, 0U, false, llvm::APFloat::roundingMode::rmNearestTiesToEven);
                break;
            case llvm::Type::FP128TyID:
                returnValStr[0]=':';
                llvm::APFloat(llvm::APFloat::IEEEquad, result.IntVal).convertToHexString(returnValStr+1, 0U, false, llvm::APFloat::roundingMode::rmNearestTiesToEven);
                break;
            case llvm::Type::IntegerTyID: // Note: LLVM does not differentiate between signed/unsiged int types
                sprintf(returnValStr, ":%s", result.IntVal.toString(16,false).c_str());
                break;
            default:
                error(std::string("ERROR, LLVM TypeID " + std::to_string(calledFunction->getReturnType()->getTypeID()) + " of result of function \"" + calledFunction->getName().str() + "\" is not supported").c_str());
        }
        strcat(msg_buffer.get(), returnValStr);

        // send length of result msg
        auto msg_buffer_length = strlen(msg_buffer.get());
        size_t send_msg_length = std::to_string(msg_buffer_length).size() + msg_buffer_length + 1;
        std::string msg_buffer_length_str = std::to_string(send_msg_length);
        num_chars = send(sockfd, msg_buffer_length_str.c_str(), msg_buffer_length_str.size(), 0);
#ifndef NDEBUG
        std::cout << getpid() << ": writing result message, length is " << msg_buffer_length_str << std::endl;
#endif
        if (num_chars < 0)
            error("ERROR, could not write to socket");
        // send result
        msg_buffer.get()[msg_buffer_length] = '\0';
        num_chars = send(sockfd, msg_buffer.get(), msg_buffer_length+1, 0);
        if (num_chars < 0)
            error("ERROR, could not write to socket");
    }
}

