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

#include "socketclient.h"

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <unistd.h>
#include <cstring>
#include <climits>
#include <cinttypes>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <iostream>
#include <string>
#include <list>

#include "llvm/IR/DerivedTypes.h"

#include "../consts.h"
#include "../common/sockethelperfunctions.h"

SocketClient::SocketClient(std::string serverName) : AbstractClient(), serverName(serverName), msg_buffer(std::shared_ptr<char>((char*)calloc(MSG_BUFFER_SIZE, sizeof(char)), &free))
{
}

SocketClient::~SocketClient()
{
    if (sockfd != -1)
        close(sockfd);
}

int SocketClient::connectToAccelerator()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); // create IP, Stream (-> TCP) socket
    if (sockfd < 0)
        error("ERROR, could not open socket");

    struct hostent *server = gethostbyname(serverName.c_str()); // get host information
    if (!server)
        error("ERROR, could not connect to host");

    struct sockaddr_in server_addr;
    bzero((char *) &server_addr, sizeof(server_addr)); // initialise server_addr (by setting it all to zero)
    server_addr.sin_family = AF_INET; // this is an IP address
    // copy server address to socket address
    bcopy((char *) server->h_addr, (char *) &server_addr.sin_addr.s_addr, server->h_length);
    server_addr.sin_port = htons(serverPort); // set server's port with correct byte order

    // Connect socket to server
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
        error("ERROR, could not connect");

    return sockfd;
}

void SocketClient::sendIR(int sockfd, const std::string IR) {
    auto msg_size_str = std::to_string(IR.size());
    if (IR.size() + msg_size_str.size() > IR_BUFFER_SIZE)
        error("ERROR, IR file bigger than buffer size");
    int num_chars = send(sockfd, msg_size_str.c_str(), msg_size_str.size(), 0);
    if (num_chars < 0)
        error("ERROR, could not write to socket");

    num_chars = send(sockfd, IR.c_str(), IR.size(), 0);
    if (num_chars < 0)
        error("ERROR, could not write to socket");
}

void SocketClient::marshallCall(const char *funcNameAndArgs, va_list args, char *msg, std::list<std::pair<void *, std::pair<llvm::Type::TypeID, unsigned> > > &pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate)
{
#ifndef NDEBUG
    std::cout << "DEBUG" << ": marshallCall(\"" << funcNameAndArgs << "\", ..., \"" << msg << "\")" << std::endl;
#endif
    // read function name and argument types from funcNameAndArgs
    std::unique_ptr<char[]> funcNameAndArgsCopy(new char[strlen(funcNameAndArgs)+1]);
    strcpy(funcNameAndArgsCopy.get(), funcNameAndArgs);
    char *funcName = strtok(funcNameAndArgsCopy.get(), ":");

    std::list<llvm::Type::TypeID> argTypes;
    std::list<unsigned int> intBitWidths;
    std::list<llvm::Type::TypeID> pointersPointedToTypeID;
    char *currArg;
    while ((currArg = strtok(nullptr, ":")) != nullptr) {
        argTypes.push_back(static_cast<llvm::Type::TypeID>(strtoul(currArg, &currArg, 10)));

        if (argTypes.back() == llvm::Type::IntegerTyID) // integers additionally need bitwidths
            intBitWidths.push_back(strtoul(++currArg, nullptr, 10));

        if (argTypes.back() == llvm::Type::PointerTyID) { // pointers additionally need typeID of type pointed to
            pointersPointedToTypeID.push_back(static_cast<llvm::Type::TypeID>(strtoul(++currArg, &currArg, 10)));
            if (pointersPointedToTypeID.back() == llvm::Type::IntegerTyID) // integers additionally need bitwidths
                intBitWidths.push_back(strtoul(++currArg, nullptr, 10));
        }
    }

    // build call: "functionName:val1:val2...:valn"
    strcpy(msg, funcName);

    int i = 0;
    auto intBWiterator = intBitWidths.begin();
    auto pointersToTyIDIterator = pointersPointedToTypeID.begin();
    llvm::Type::TypeID pointingToTyID = llvm::Type::NumTypeIDs; // = explicitly unset
    unsigned pointingToIntBW;
    void* ptrPending = nullptr;
    for (const auto& currArg : argTypes) {
        i++;
        char currVal[MAX_VAL_SIZE];
        switch (currArg) {
            case llvm::Type::PointerTyID: {
                pointingToTyID = *pointersToTyIDIterator++;
                pointingToIntBW = *intBWiterator++;
                std::cout << "DEBUG" << ": argument " << i << " is pointer to type with TypeID " << pointingToTyID << ", assuming argument " << i+1 << " is int, giving number of elements in area pointed to\n";
                std::cout << "DEBUG" << ": area will be marshalled with handling of next argument\n";
                ptrPending = va_arg(args, void*);
                auto ptrPointingToType = std::make_pair(pointingToTyID, pointingToIntBW);
                pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate.push_back(std::pair<void *, std::pair<llvm::Type::TypeID, unsigned>>(ptrPending, ptrPointingToType));
                currVal[0] = '\0';
                break;
            }
            case llvm::Type::FloatTyID:
            case llvm::Type::DoubleTyID:
                sprintf(currVal, ":%la", va_arg(args, double));
                break;
            case llvm::Type::X86_FP80TyID:
            case llvm::Type::FP128TyID:
                sprintf(currVal, ":%La", va_arg(args, long double));
                break;
            case llvm::Type::IntegerTyID: // Note: LLVM does not differentiate between signed/unsiged int types
                switch (*intBWiterator++) {
                    case 32: {
                        int32_t tmp = va_arg(args, int32_t);
                        if (pointingToTyID != llvm::Type::NumTypeIDs) {
                            SocketHelperFunctions::marshallArrayOfSizeAndTypeIntoBuffer(ptrPending, tmp, std::pair<llvm::Type::TypeID, unsigned>(pointingToTyID, pointingToIntBW), msg);
                            pointingToTyID = llvm::Type::NumTypeIDs;
                            pointingToIntBW = 0U;
                        }
                        sprintf(currVal, ":%" PRIx32, tmp);
                        break;
                    }
                    case 64: {
                        int64_t tmp = va_arg(args, int64_t);
                        if (pointingToTyID != llvm::Type::NumTypeIDs) {
                            SocketHelperFunctions::marshallArrayOfSizeAndTypeIntoBuffer(ptrPending, tmp, std::pair<llvm::Type::TypeID, unsigned>(pointingToTyID, pointingToIntBW), msg);
                            pointingToTyID = llvm::Type::NumTypeIDs;
                            pointingToIntBW = 0U;
                        }
                        sprintf(currVal, ":%" PRIx64, tmp);
                        break;
                    }
                    default: {
                        int tmp = va_arg(args, int);
                        if (pointingToTyID != llvm::Type::NumTypeIDs) {
                            SocketHelperFunctions::marshallArrayOfSizeAndTypeIntoBuffer(ptrPending, tmp, std::pair<llvm::Type::TypeID, unsigned>(pointingToTyID, pointingToIntBW), msg);
                            pointingToTyID = llvm::Type::NumTypeIDs;
                            pointingToIntBW = 0U;
                        }
                        sprintf(currVal, ":%x", tmp);
                        break;
                    }
                }
                break;
            default:
                error(std::string("ERROR, LLVM TypeID " + std::to_string(currArg) + " of argument " + std::to_string(i) + " in function \"" + funcName + "\" is not supported").c_str());
        }
        strcat(msg, currVal);
    }
}

void SocketClient::initialiseAccelerationWithIR(const std::string &IR)
{
    sockfd = connectToAccelerator();
    sendIR(sockfd, IR);

    const auto maxRdyMsgLen = std::to_string(std::numeric_limits<long>::max()).size()*2+1;
    char rdy_msg_buffer[maxRdyMsgLen];
    int num_chars = recv(sockfd, rdy_msg_buffer, maxRdyMsgLen, 0);
    if (num_chars == 0) {
        std::cout << "ERROR: " << "Lost connection to server \n";
        exit(0);
    }

    char* rdy_msg_buffer_ptr = &rdy_msg_buffer[0];
    TimeDiffOpt = strtol(rdy_msg_buffer_ptr, &rdy_msg_buffer_ptr, 10);
    rdy_msg_buffer_ptr++;
    TimeDiffInit = strtol(rdy_msg_buffer_ptr, &rdy_msg_buffer_ptr, 10);
}

void SocketClient::callAcc(const char *retTypeFuncNameArgTypes, va_list args)
{
    assert(sockfd != -1 && "Connection to server uninitialised!");

    // initialise msg_buffer
    bzero(msg_buffer.get(), MSG_BUFFER_SIZE);


    // get result type and container to write result value to
    char* behindResTypePtr;
    unsigned int retTypeBitWidth;
    auto retType = static_cast<llvm::Type::TypeID>(strtol(retTypeFuncNameArgTypes, &behindResTypePtr, 10));
    if (retType == llvm::Type::IntegerTyID)
        retTypeBitWidth = strtol(++behindResTypePtr, &behindResTypePtr, 10);
    behindResTypePtr++;

    void* retContainerPtr;
    if (retType != llvm::Type::VoidTyID)
        retContainerPtr = va_arg(args, void*);

    // initialise list to keep arguments which might change during call, write call
    std::list<std::pair<void*, std::pair<llvm::Type::TypeID, unsigned>>> pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate;
    marshallCall(behindResTypePtr, args, msg_buffer.get(), pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate);

    // send length of call msg
    auto msg_buffer_length = strlen(msg_buffer.get());
    size_t send_msg_length = std::to_string(msg_buffer_length).size() + msg_buffer_length + 1;
    std::string msg_buffer_length_str = std::to_string(send_msg_length);
    int num_chars = send(sockfd, msg_buffer_length_str.c_str(), msg_buffer_length_str.size(), 0);
    if (num_chars < 0)
        error("ERROR, could not write to socket");

    // send call msg
    msg_buffer.get()[msg_buffer_length] = '\0';
    num_chars = send(sockfd, msg_buffer.get(), msg_buffer_length + 1, 0);
    if (num_chars < 0)
        error("ERROR, could not write to socket");

    // reset msg_buffer to read result from socket
    bzero(msg_buffer.get(), MSG_BUFFER_SIZE);

    // first acquire result message length
    unsigned msg_length;
    auto UINT_MAX_str_len = std::to_string(UINT_MAX).length();
    num_chars = recv(sockfd, msg_buffer.get(), UINT_MAX_str_len + 1, 0);
    if (num_chars == 0) {
        std::cout << "ERROR, lost connection to server \n";
        exit(0);
    }
    char* msg_buffer_after_length_ptr;
    msg_length = strtoul(msg_buffer.get(), &msg_buffer_after_length_ptr, 10);
    // then get the message, some parts might have come with the length previously
    auto msg_part_length = strlen(msg_buffer.get());
    if (num_chars < msg_length)
        num_chars = recv(sockfd, msg_buffer.get() + msg_part_length, msg_length - msg_part_length, MSG_WAITALL);
    if (num_chars < 0)
        error("ERROR, could not read from socket");
    std::cout << "DEBUG" << ": got result of size " << num_chars + msg_part_length << std::endl;
    char* msg_buffer_ptr = ((char*)msg_buffer_after_length_ptr) + 1;

    // get time measures
    TimeDiffLastExecution = strtol(msg_buffer_ptr, &msg_buffer_ptr, 10);
    msg_buffer_ptr++;

    // unmarshal changes to pointed to memory areas
    std::cout << "DEBUG" << ": updating " << pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate.size() << " arguments\n";
    for (const auto& pointerAndType : pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate)
        SocketHelperFunctions::unmarshalArrayFromBufferAsTypeIntoExistingMemory(msg_buffer_ptr, pointerAndType.second, pointerAndType.first);
    if (pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate.size())
        msg_buffer_ptr++;

    // interpret result with typeinfo from retType
    switch (retType) {
        case llvm::Type::VoidTyID:
            std::cout << "DEBUG" << ": got void return\n";
            break;
        case llvm::Type::FloatTyID:
            sscanf(msg_buffer_ptr, "%a", (float*)retContainerPtr);
        case llvm::Type::DoubleTyID:
            sscanf(msg_buffer_ptr, "%la", (double*)retContainerPtr);
            break;
        case llvm::Type::X86_FP80TyID:
        case llvm::Type::FP128TyID:
            sscanf(msg_buffer_ptr, "%La", (long double*)retContainerPtr);
            break;
        case llvm::Type::IntegerTyID: // Note: LLVM does not differentiate between signed/unsiged int types
            switch (retTypeBitWidth) {
                case 8:
                    sscanf(msg_buffer_ptr, "%" SCNx8, (uint8_t*)retContainerPtr);
                    break;
                case 16:
                    sscanf(msg_buffer_ptr, "%" SCNx16, (uint16_t*)retContainerPtr);
                    break;
                case 32:
                    sscanf(msg_buffer_ptr, "%" SCNx32, (uint32_t*)retContainerPtr);
                    break;
                case 64:
                    sscanf(msg_buffer_ptr, "%" SCNx64, (uint64_t*)retContainerPtr);
                    break;
                default:
                    error(std::string("ERROR, integer bitwidth of " + std::to_string(retTypeBitWidth) + " not supported").c_str());
                    break;
            }
            break;
        default:
            error(std::string("ERROR, LLVM TypeID " + std::to_string(retType) + " of function return value is not supported").c_str());
    }
}
