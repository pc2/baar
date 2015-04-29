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

#include "../common/mpihelper.h"
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
    #ifndef NDEBUG
        std::cout << "DEBUG: Sending IR\n" << std::endl;
    #endif

    int myrank;
    MPI_Comm_rank(server, &myrank);
    int mpi_client_tag = MPI_CLIENT_TAG;
    int mpi_client_rank =0;
    
    // TODO: check this!
    if(myrank==0)
          mpi_client_rank = 1;

    MPI_Send((void*) IR.c_str(), IR.size() , MPI_CHAR, mpi_client_rank, mpi_client_tag, server);

    #ifndef NDEBUG
        std::cout << "DEBUG: IR Sent\n" << std::endl;
    #endif
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
}

void SocketClient::marshallMPICall(const char *funcNameAndArgs, va_list args, char *msg, std::list<std::pair<void *, std::pair<llvm::Type::TypeID, unsigned> > > &pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate)
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

    //Set up the structure
    //allocate memory for struct
    struct ArgumentList argList[MAX_NUMBER_OF_ARGUMENTS];
    MPI_Status status;

    int myrank;
    MPI_Comm_rank(server, &myrank);

    //Create the structure
    int structSize=0;
    int mpi_client_tag = MPI_CLIENT_TAG;
    
    int mpi_client_rank =0;
    
    // TODO: check this! 
    if(myrank==0)
          mpi_client_rank = 1;

    //Data structure to hold the addresses of the data
    struct mpi_send_data {
        void * ptrToAdddress;
        bool free;
    } ptrToAdddress[MAX_NUMBER_OF_ARGUMENTS];

    int ptrToAdddressIterator=0;

    int i = 0;
    auto intBWiterator = intBitWidths.begin();
    auto pointersToTyIDIterator = pointersPointedToTypeID.begin();
    llvm::Type::TypeID pointingToTyID = llvm::Type::NumTypeIDs; // = explicitly unset
    unsigned pointingToIntBW;
    void* ptrPending = nullptr;
    void *tmpdata = nullptr;

    for (const auto& currArg : argTypes) {
        i++;
        char currVal[MAX_VAL_SIZE];
        switch (currArg) {
            case llvm::Type::PointerTyID: {
                pointingToTyID = *pointersToTyIDIterator++;
                // Only get bit width, if pointer points to interger type.
                if(pointingToTyID == llvm::Type::IntegerTyID) {
                    pointingToIntBW = *intBWiterator++;
                } else {
                    pointingToIntBW = 0;
                }
                #ifndef NDEBUG
                    std::cout << "DEBUG" << ": argument " << i << " is pointer to type with TypeID " << pointingToTyID << " (width: " << pointingToIntBW << "), assuming argument " << i+1 << " is int, giving number of elements in area pointed to\n";
                    std::cout << "DEBUG" << ": area will be marshalled with handling of next argument\n";
                #endif
                ptrPending = va_arg(args, void*);
                auto ptrPointingToType = std::make_pair(pointingToTyID, pointingToIntBW);
                pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate.push_back(std::pair<void *, std::pair<llvm::Type::TypeID, unsigned>>(ptrPending, ptrPointingToType));
                currVal[0] = '\0';
                break;
            }
            case llvm::Type::FloatTyID:
            case llvm::Type::DoubleTyID:
                tmpdata = malloc(sizeof(double));
                *(double *)tmpdata = va_arg(args, double);

                argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_DOUBLE;
                argList[ptrToAdddressIterator].sizeOfArg = 1;
                ptrToAdddress[ptrToAdddressIterator].free = 1;
                ptrToAdddress[ptrToAdddressIterator].ptrToAdddress = tmpdata;
                ptrToAdddressIterator++;

                break;
            case llvm::Type::X86_FP80TyID:
            case llvm::Type::FP128TyID:
                tmpdata = malloc(sizeof(long double));
                *(long double *)tmpdata = va_arg(args, long double);

                argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_LONG_DOUBLE;
                argList[ptrToAdddressIterator].sizeOfArg = 1;
                ptrToAdddress[ptrToAdddressIterator].free = 1;
                ptrToAdddress[ptrToAdddressIterator].ptrToAdddress = tmpdata;
                ptrToAdddressIterator++;

                break;
            case llvm::Type::IntegerTyID: // Note: LLVM does not differentiate between signed/unsiged int types
                switch (*intBWiterator++) {
                    case 32: {
                        tmpdata = malloc(sizeof(int32_t));
                        *(int32_t *)tmpdata = va_arg(args, int32_t);

                        if (pointingToTyID != llvm::Type::NumTypeIDs) {
                            switch(pointingToTyID) {
                                case llvm::Type::IntegerTyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_INT;
                                    break;
                                case llvm::Type::FloatTyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_FLOAT;
                                    break;

                                case llvm::Type::DoubleTyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_DOUBLE;
                                    break;
                                case llvm::Type::FP128TyID:
                                case llvm::Type::X86_FP80TyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_LONG_DOUBLE;
                                    break;
                                default:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_INT;
                            }
                            argList[ptrToAdddressIterator].sizeOfArg = (long) *(int32_t *)tmpdata;
                            ptrToAdddress[ptrToAdddressIterator].free=0;
                            ptrToAdddress[ptrToAdddressIterator].ptrToAdddress = ptrPending;
                            ptrToAdddressIterator++;

                            //SocketHelperFunctions::marshallArrayOfSizeAndTypeIntoBuffer(ptrPending, tmp, std::pair<llvm::Type::TypeID, unsigned>(pointingToTyID, pointingToIntBW), msg);
                            pointingToTyID = llvm::Type::NumTypeIDs;
                            pointingToIntBW = 0U;
                        }

                        argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_INT;
                        argList[ptrToAdddressIterator].sizeOfArg = 1;
                        ptrToAdddress[ptrToAdddressIterator].free = 1;
                        ptrToAdddress[ptrToAdddressIterator].ptrToAdddress = tmpdata;
                        ptrToAdddressIterator++;

                        break;
                    }
                    case 64: {
                        tmpdata = malloc(sizeof(int64_t));
                        *(int64_t *)tmpdata = va_arg(args, int64_t);

                        if (pointingToTyID != llvm::Type::NumTypeIDs) {
                            switch(pointingToTyID) {
                                case llvm::Type::IntegerTyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_INT;
                                    break;
                                case llvm::Type::FloatTyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_FLOAT;
                                    break;

                                case llvm::Type::DoubleTyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_DOUBLE;
                                    break;
                                case llvm::Type::FP128TyID:
                                case llvm::Type::X86_FP80TyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_LONG_DOUBLE;
                                    break;
                                default:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_INT;
                            }

                            argList[ptrToAdddressIterator].sizeOfArg = (long) *(int64_t *)tmpdata;
                            ptrToAdddress[ptrToAdddressIterator].free=0;
                            ptrToAdddress[ptrToAdddressIterator].ptrToAdddress = ptrPending;
                            ptrToAdddressIterator++;

                            //SocketHelperFunctions::marshallArrayOfSizeAndTypeIntoBuffer(ptrPending, tmp, std::pair<llvm::Type::TypeID, unsigned>(pointingToTyID, pointingToIntBW), msg);
                            pointingToTyID = llvm::Type::NumTypeIDs;
                            pointingToIntBW = 0U;
                        }
                        argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_LONG;
                        argList[ptrToAdddressIterator].sizeOfArg = 1;
                        ptrToAdddress[ptrToAdddressIterator].free = 1;
                        ptrToAdddress[ptrToAdddressIterator].ptrToAdddress = tmpdata;
                        ptrToAdddressIterator++;

                        break;
                    }
                    default: {
                        void *tmpdata = malloc(sizeof(int));
                        *(int *)tmpdata = va_arg(args, int);

                        if (pointingToTyID != llvm::Type::NumTypeIDs) {
                            switch(pointingToTyID) {
                                case llvm::Type::IntegerTyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_INT;
                                    break;
                                case llvm::Type::FloatTyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_FLOAT;
                                    break;

                                case llvm::Type::DoubleTyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_DOUBLE;
                                    break;
                                case llvm::Type::FP128TyID:
                                case llvm::Type::X86_FP80TyID:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_LONG_DOUBLE;
                                    break;
                                default:
                                    argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_INT;
                            }

                            argList[ptrToAdddressIterator].sizeOfArg = (long) *(int *)tmpdata;
                            ptrToAdddress[ptrToAdddressIterator].free=0;
                            ptrToAdddress[ptrToAdddressIterator].ptrToAdddress = ptrPending;
                            ptrToAdddressIterator++;

                            //SocketHelperFunctions::marshallArrayOfSizeAndTypeIntoBuffer(ptrPending, tmp, std::pair<llvm::Type::TypeID, unsigned>(pointingToTyID, pointingToIntBW), msg);
                            pointingToTyID = llvm::Type::NumTypeIDs;
                            pointingToIntBW = 0U;
                        }
                        argList[ptrToAdddressIterator].typeofArg = ENUM_MPI_INT;
                        argList[ptrToAdddressIterator].sizeOfArg = 1;
                        ptrToAdddress[ptrToAdddressIterator].free = 1;
                        ptrToAdddress[ptrToAdddressIterator].ptrToAdddress = tmpdata            ;
                        ptrToAdddressIterator++;

                        break;
                    }
                }
                break;
            default:
                error(std::string("ERROR, LLVM TypeID " + std::to_string(currArg) + " of argument " + std::to_string(i) + " in function \"" + funcName + "\" is not supported").c_str());
        }
    }

    //Send the MPI_Header / structure
    #ifndef NDEBUG
        std::cout << "\nMPI CLIENT: Sending MPI Header";
        std::cout.flush();
        
        //Display sent DS
        for (int i=0; i<ptrToAdddressIterator; i++) {
            std::cout <<  "\n MPI Sent DS : Size : " << argList[i].sizeOfArg << "  Type" << argList[i].typeofArg ;
            std::cout.flush();
        }
    #endif

    structSize = ptrToAdddressIterator;
    MPI_Send(argList, structSize, ArgListType, mpi_client_rank, mpi_client_tag, server);

    #ifndef NDEBUG
        std::cout << "\nMPI CLIENT: Sending MPI Data to server";
        std::cout.flush();
    #endif
    
    ///MPI_DATA_MOVEMENT
    //Send the MPI data
    for(int i=0; i<ptrToAdddressIterator; i++) {
        switch(argList[i].typeofArg) {
            case ENUM_MPI_CHAR:
                MPI_Send(ptrToAdddress[i].ptrToAdddress, argList[i].sizeOfArg, MPI_CHAR , mpi_client_rank, mpi_client_tag, server);
                break;
            case ENUM_MPI_SHORT:
                MPI_Send(ptrToAdddress[i].ptrToAdddress, argList[i].sizeOfArg, MPI_SHORT , mpi_client_rank, mpi_client_tag, server);
                break;
            case ENUM_MPI_INT:
                MPI_Send(ptrToAdddress[i].ptrToAdddress, argList[i].sizeOfArg, MPI_INT , mpi_client_rank, mpi_client_tag, server);
                break;
            case ENUM_MPI_LONG:
                MPI_Send(ptrToAdddress[i].ptrToAdddress, argList[i].sizeOfArg, MPI_LONG , mpi_client_rank, mpi_client_tag, server);
                break;
            case ENUM_MPI_LONG_LONG:
                MPI_Send(ptrToAdddress[i].ptrToAdddress, argList[i].sizeOfArg, MPI_LONG_LONG, mpi_client_rank, mpi_client_tag, server);
            case ENUM_MPI_FLOAT:
                MPI_Send(ptrToAdddress[i].ptrToAdddress, argList[i].sizeOfArg, MPI_FLOAT , mpi_client_rank, mpi_client_tag, server);
                break;
            case ENUM_MPI_DOUBLE:
                MPI_Send(ptrToAdddress[i].ptrToAdddress, argList[i].sizeOfArg, MPI_DOUBLE , mpi_client_rank, mpi_client_tag, server);
                break;
            case ENUM_MPI_LONG_DOUBLE:
                MPI_Send(ptrToAdddress[i].ptrToAdddress, argList[i].sizeOfArg, MPI_LONG_DOUBLE , mpi_client_rank, mpi_client_tag, server);
                break;
            default:
                std::cout << "Data-type : " << argList[i].typeofArg << "%d is not supported";
                std::cout.flush();
        }
        //free if required
        if(ptrToAdddress[i].free) {
            free(ptrToAdddress[i].ptrToAdddress);
        }
    } 
    
    #ifndef NDEBUG
        std::cout << "\nMPI CLIENT: Completed sending MPI Data to server";
        std::cout.flush();
    #endif    
}

void SocketClient::initialiseAccelerationWithIR(const std::string &IR)
{
    sockfd = connectToAccelerator();

    // MPI_CONNECTION_INIT
    int argc=0;
    MPI_Init( &argc, NULL ); 
    server = MPI_COMM_WORLD;

    sendIR(sockfd, IR);
    
    MPI_Status status;
    int mpi_client_tag = MPI_CLIENT_TAG;
    int incomingMessageSize=0;    
    MPI_Probe(MPI_ANY_SOURCE, mpi_client_tag, server, &status);
    MPI_Get_count(&status,MPI_CHAR,&incomingMessageSize);    
    char *rdy_msg_buffer = (char *) calloc(incomingMessageSize + 1 , sizeof(char));    
    MPI_Recv(rdy_msg_buffer, incomingMessageSize + 1, MPI_CHAR, MPI_ANY_SOURCE, mpi_client_tag, server, &status);
    
    char* rdy_msg_buffer_ptr = rdy_msg_buffer;
    TimeDiffOpt = strtol(rdy_msg_buffer_ptr, &rdy_msg_buffer_ptr, 10);
    rdy_msg_buffer_ptr++;
    TimeDiffInit = strtol(rdy_msg_buffer_ptr, &rdy_msg_buffer_ptr, 10);
}

void SocketClient::callAcc(const char *retTypeFuncNameArgTypes, va_list args)
{
    assert(sockfd != -1 && "Connection to server uninitialised!");
    
    // Create MPI Structure
    int sizeOfData;
    MPI_Type_size( MPI_INT,&sizeOfData );
    int array_of_block_lengths[2] = {1, 1};
    MPI_Aint array_of_displacements[2] = {0, sizeOfData};
    MPI_Datatype array_of_types[2] = { MPI_INT, MPI_INT };

    MPI_Type_create_struct(2, array_of_block_lengths, array_of_displacements, array_of_types, &ArgListType);
    MPI_Type_commit(&ArgListType);
    // End of MPI struct

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

    #ifndef NDEBUG
        std::cout << "<marshalled: " << retTypeFuncNameArgTypes << " \n";
    #endif
    marshallCall(behindResTypePtr, args, msg_buffer.get(), pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate);
    #ifndef NDEBUG
        std::cout << "<marshalled: \n";
    #endif

    // send length of call msg
    auto msg_buffer_length = strlen(msg_buffer.get());

    // send call msg
    msg_buffer.get()[msg_buffer_length] = '\0';
    int num_chars = send(sockfd, msg_buffer.get(), msg_buffer_length + 1, 0);
    if (num_chars < 0)
        error("ERROR, could not write to socket");
        
    //MPI Send
    marshallMPICall(behindResTypePtr, args, msg_buffer.get(), pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate);

    // reset msg_buffer to read result from socket
    bzero(msg_buffer.get(), MSG_BUFFER_SIZE);
    
    //MPI_DATA_MOVEMENT
    #ifndef NDEBUG
        std::cout << "\nMPI CLIENT: Getting data from server";
        std::cout.flush();
    #endif   

    //Get the updated content from the server
    //Create the MPI data structure

    //allocate memory for struct
    struct ArgumentList argList[MAX_NUMBER_OF_ARGUMENTS];
    MPI_Status status;

    //Create the structure
    int structSize=0;
    int mpi_client_tag = MPI_CLIENT_TAG;

    #ifndef NDEBUG
        std::cout << "\nMPI CLIENT: Waiting for Header\n";
        std::cout.flush();
    #endif

    MPI_Recv(argList, MAX_NUMBER_OF_ARGUMENTS, ArgListType, MPI_ANY_SOURCE, mpi_client_tag, server, &status);
    
    #ifndef NDEBUG
        std::cout << "\nMPI CLIENT: Header Recieved\n";
        std::cout.flush();
    #endif

    int recordCount=0;
    MPI_Get_count(&status,ArgListType,&recordCount);
        
    #ifndef NDEBUG 
        //Display recieved DS
        for (int i=0; i<recordCount; i++) {
            std::cout <<  "\n MPI Recieved DS : Size : " << argList[i].sizeOfArg << "  Type" << argList[i].typeofArg ;
            std::cout.flush();
        }
    #endif
    
    assert(pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate.size() == recordCount && "Number of arrays to update do not match");

    #ifndef NDEBUG
        std::cout << "\nMPI CLIENT: Updting "<< recordCount << "arguments\n";
        std::cout << "MPI CLIENT: Waiting for data\n";
        std::cout.flush();
    #endif

    for (const auto& pointerAndType : pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate) {
        MPI_DATATYPE_ENUM mpi_dataType;
        switch(pointerAndType.second.first) {

        case llvm::Type::IntegerTyID:
            MPI_Recv(pointerAndType.first, MPI_MAX_RECV_BUFFER_SIZE, MPI_INT , MPI_ANY_SOURCE, mpi_client_tag, server, &status);
            break;

        case llvm::Type::FloatTyID:
            MPI_Recv(pointerAndType.first, MPI_MAX_RECV_BUFFER_SIZE, MPI_FLOAT , MPI_ANY_SOURCE, mpi_client_tag, server, &status);
            break;
        case llvm::Type::DoubleTyID:
            MPI_Recv(pointerAndType.first, MPI_MAX_RECV_BUFFER_SIZE, MPI_DOUBLE , MPI_ANY_SOURCE, mpi_client_tag, server, &status);
            break;
        case llvm::Type::FP128TyID:
        case llvm::Type::X86_FP80TyID:
            MPI_Recv(pointerAndType.first, MPI_MAX_RECV_BUFFER_SIZE, MPI_LONG_DOUBLE , MPI_ANY_SOURCE, mpi_client_tag, server, &status);
            break;
        default:
            MPI_Recv(pointerAndType.first, MPI_MAX_RECV_BUFFER_SIZE, MPI_INT , MPI_ANY_SOURCE, mpi_client_tag, server, &status);
        }
    }
    
    MPI_Type_free(&ArgListType);
    
    #ifndef NDEBUG
        std::cout << "\nMPI CLIENT: Data from server recieved";
        std::cout.flush();
    #endif 
    
    auto UINT_MAX_str_len = std::to_string(UINT_MAX).length();
    MPI_Recv(msg_buffer.get(), UINT_MAX_str_len + 1, MPI_CHAR, MPI_ANY_SOURCE, mpi_client_tag, server, &status);
    
    char* msg_buffer_ptr = msg_buffer.get();
    
    // get time measures
    TimeDiffLastExecution = strtol(msg_buffer_ptr, &msg_buffer_ptr, 10);
    msg_buffer_ptr++;
    
    //TODO:: Check this
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

    //MPI_CONNECTION_INIT
    // TODO: check this
    MPI_Finalize();
    
}