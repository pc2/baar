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

SocketServer::~SocketServer(){    
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
        {
            close(newsockfd);
        }
    }
}

void SocketServer::cleanupCommunication()
{
    close(sockfd);
}

void SocketServer::unmarshalCallArgs( char *buffer, int functionName_offset, llvm::Function *calledFunction, std::vector<llvm::GenericValue> &args, std::list<std::vector<llvm::GenericValue>::size_type> &indexesOfPointersInArgs)
{
    llvm::FunctionType *CalledFuncType = calledFunction->getFunctionType();
    int numArgs = CalledFuncType->getNumParams();


    int mpi_server_tag = MPI_SERVER_TAG;
    MPI_Status status;
  
    #ifndef NDEBUG
        std::cout << "\nMPI SERVER: Waiting for Header";
        std::cout.flush();
    #endif
    MPI_Recv(argumentList, MAX_NUMBER_OF_ARGUMENTS, ArgListType, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
    #ifndef NDEBUG
        std::cout << "\nMPI SERVER: Header Recieved";
        std::cout.flush();
    #endif

    int recordCount=0;
    MPI_Get_count(&status,ArgListType,&recordCount);

    #ifndef NDEBUG
        //Display sent DS
        for (int i=0; i<recordCount; i++) {
            std::cout <<  "\n MPI Recieved DS : Size : " << argumentList[i].sizeOfArg << "  Type" << argumentList[i].typeofArg ;
            std::cout.flush();
        }
    #endif

    assert(numArgs == recordCount && "Number of function arguments do not match");
    void *array;

    //MPI_DATA_MOVEMENT
    for (int i = 0; i < numArgs; i++) {
        llvm::GenericValue CurrArg;
        llvm::Type* CurrType = CalledFuncType->getParamType(i);

        long arraySize=argumentList[i].sizeOfArg;

        #ifndef NDEBUG
            std::cout << "\nProcessing arg -> "<< arraySize << i << "\n";
            std::cout.flush();
        #endif

        switch (CurrType->getTypeID()) {
            case llvm::Type::PointerTyID:
                indexesOfPointersInArgs.push_back(i);

                //Get each argument
                #ifndef NDEBUG
                        std::cout << "\nProcessing parameters size -> "<< arraySize <<", Type -> " << argumentList[i].typeofArg << "\n";
                        std::cout.flush();
                #endif

                if(arraySize>0) {
                    //array
                    switch(argumentList[i].typeofArg) {
                    case ENUM_MPI_CHAR:
                        array = malloc(arraySize*sizeof(char));
                        MPI_Recv(array, arraySize, MPI_CHAR, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                        CurrArg.PointerVal=array;
                        break;
                    case ENUM_MPI_SHORT:
                        array = malloc(arraySize*sizeof(short));
                        MPI_Recv(array, arraySize, MPI_SHORT, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                        CurrArg.PointerVal=array;
                        break;
                    case ENUM_MPI_INT:
                        array = malloc(arraySize*sizeof(int));
                        MPI_Recv(array, arraySize, MPI_INT, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                        CurrArg.PointerVal=array;
                        break;
                    case ENUM_MPI_LONG:
                        array = malloc(arraySize*sizeof(long));
                        MPI_Recv(array, arraySize, MPI_LONG, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                        CurrArg.PointerVal=array;
                        break;
                    case ENUM_MPI_LONG_LONG:
                        array = malloc(arraySize*sizeof(long long));
                        MPI_Recv(array, arraySize, MPI_LONG_LONG, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                        CurrArg.PointerVal=array;
                        break;
                    case ENUM_MPI_FLOAT:
                        array = malloc(arraySize*sizeof(float));
                        MPI_Recv(array, arraySize, MPI_FLOAT, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                        CurrArg.PointerVal=array;
                        break;
                    case ENUM_MPI_DOUBLE:
                        array = malloc(arraySize*sizeof(double));
                        MPI_Recv(array, arraySize, MPI_DOUBLE, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                        CurrArg.PointerVal=array;
                        break;
                    case ENUM_MPI_LONG_DOUBLE:
                        array = malloc(arraySize*sizeof(long double));
                        MPI_Recv(array, arraySize, MPI_LONG_DOUBLE, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                        CurrArg.PointerVal=array;
                        break;
                    default:
                        std::cout << "Data-type : " << argumentList[i].typeofArg << "%d is not supported";
                        std::cout.flush();
                    }
                }

                break;
            case llvm::Type::FloatTyID:
                MPI_Recv(&CurrArg.FloatVal, 1, MPI_FLOAT, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                break;
            case llvm::Type::DoubleTyID:
                MPI_Recv(&CurrArg.DoubleVal, 1, MPI_DOUBLE, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                break;
            case llvm::Type::X86_FP80TyID: {
                //TODO
                //             size_t valstring_size = strcspn(buffer, ":\0");
                //             std::string valstring(buffer, valstring_size);
                //             buffer += valstring_size;
                //             llvm::APFloat bigval(llvm::APFloat::x87DoubleExtended, valstring);
                //             CurrArg.IntVal = bigval.bitcastToAPInt();
                break;
            }
            case llvm::Type::FP128TyID: {
                //TODO
                //             size_t valstring_size = strcspn(buffer, ":\0");
                //             std::string valstring(buffer, valstring_size);
                //             buffer += valstring_size;
                //             llvm::APFloat bigval(llvm::APFloat::IEEEquad, valstring);
                //             CurrArg.IntVal = bigval.bitcastToAPInt();
                break;
            }
            case llvm::Type::IntegerTyID: { // Note: LLVM does not differentiate between signed/unsiged int types
                void *tmpInt = nullptr;
                tmpInt = malloc(sizeof(int));
                MPI_Recv(tmpInt, 1, MPI_INT, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
                CurrArg.IntVal = *(int*)tmpInt;
                free(tmpInt);
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
                std::cout << "ptr " ;//<< pointerToTy->getTypeID();
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
            default:
                std::cout << "TODO";
            }
            std::cout << ((i == numArgs-1)? ")'.\nDEBUG: Note, LLVM represents all ints as uint64_t, long double as two uint64_t internally.\n" : ", ");
        #endif
    }

    #ifndef NDEBUG
        if (numArgs == 0)
            std::cout << ")'.\n";
    #endif
}

void SocketServer::handle_conn(int sockfd)
{
    //MPI_CONNECTION_INIT
    // TODO: check this! 
    int argc = 0;
    
    #ifndef NDEBUG
        std::cout << "INFO" << ": trying MPI_Init " << std::endl;
    #endif
    MPI_Init( &argc, NULL );
    #ifndef NDEBUG
        std::cout << "INFO" << ": ... done " << std::endl;
    #endif
    
    // Create MPI Structure
    int sizeOfData;
    MPI_Type_size( MPI_INT,&sizeOfData );
    int array_of_block_lengths[2] = {1, 1};
    MPI_Aint array_of_displacements[2] = {0, sizeOfData};
    MPI_Datatype array_of_types[2] = { MPI_INT, MPI_INT };

    MPI_Type_create_struct(2, array_of_block_lengths, array_of_displacements, array_of_types, &ArgListType);
    MPI_Type_commit(&ArgListType);
    // End of MPI struct
    
    client = MPI_COMM_WORLD;

    #ifndef NDEBUG
        std::cout << "DEBUG: Waiting for IR\n" << std::endl;
    #endif
    
    MPI_Status status;
    int mpi_server_tag = MPI_SERVER_TAG;
    int myrank;
    MPI_Comm_rank(client, &myrank);
    int mpi_server_rank =0;
    
    // TODO: check this! 
    if(myrank==0)
          mpi_server_rank = 1;
    
    int incomingMessageSize=0;    
    MPI_Probe(MPI_ANY_SOURCE, mpi_server_tag, client, &status);
    MPI_Get_count(&status,MPI_CHAR,&incomingMessageSize);    
    char *module_ir_buffer = (char *) calloc(incomingMessageSize + 1 , sizeof(char));    
    MPI_Recv(module_ir_buffer, incomingMessageSize + 1, MPI_CHAR, MPI_ANY_SOURCE, mpi_server_tag, client, &status);
    
    #ifndef NDEBUG
        std::cout << "DEBUG: Recieved IR\n" << std::endl;
    #endif
  
    auto backend = parseIRtoBackend(module_ir_buffer);
    // notify client that calls can be accepted now by sending time taken for optimizing module and initialising backend
    const std::string readyStr(std::to_string(TimeDiffOpt.count()) + ":" + std::to_string(TimeDiffInit.count()));
    MPI_Send((void *)readyStr.c_str(), readyStr.size() , MPI_CHAR, mpi_server_rank, mpi_server_tag, client);
    free(module_ir_buffer);

    // initialise msg_buffer
    std::shared_ptr<char> msg_buffer((char*)calloc(MSG_BUFFER_SIZE, sizeof(char)), &free);
    while (1) {
        bzero(msg_buffer.get(), MSG_BUFFER_SIZE);
        // first acquire message length
        unsigned msg_length;
        auto UINT_MAX_str_len = std::to_string(UINT_MAX).length();
        int num_chars = recv(sockfd, msg_buffer.get(), UINT_MAX_str_len + 1, 0);
    
        if (num_chars == 0) {
            std::cout << "Client assigned to process " << getpid() << " has closed its socket 3 \n";
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
        llvm::GenericValue result = handleCall(backend.get(), msg_buffer.get(), calledFunction, args, indexesOfPointersInArgs);

        // reset buffer and write time taken to buffer
        bzero(msg_buffer.get(), MSG_BUFFER_SIZE);
        sprintf(msg_buffer.get(), ";%ld", (long)TimeDiffLastExecution.count());

        //MPI_DATA_MOVEMENT
        //Send data back to the client
        //Create the MPI data structure
        //allocate memory for struct    
        #ifndef TIMING 
            auto StartTime = std::chrono::high_resolution_clock::now();
        #endif  
    
        struct ArgumentList argList[MAX_NUMBER_OF_ARGUMENTS];
        MPI_Status status;

        //Create the structure
        int structSize=0;
    
        for (const auto& indexOfPtr : indexesOfPointersInArgs) {
            auto paramType = calledFunction->getFunctionType()->getParamType(indexOfPtr);
            while (paramType->getTypeID() == llvm::Type::ArrayTyID || paramType->getTypeID() == llvm::Type::PointerTyID)
            paramType = llvm::cast<llvm::SequentialType>(paramType)->getElementType();

            if (paramType->getTypeID() == llvm::Type::IntegerTyID) {
                argList[structSize].typeofArg = ENUM_MPI_INT;
            } else {
                argList[structSize].typeofArg = ENUM_MPI_DOUBLE;
            }
            argList[structSize].sizeOfArg =argumentList[indexOfPtr].sizeOfArg;
            structSize++;
        }

        #ifndef NDEBUG
            std::cout << "\nMPI SERVER: Sending message back from server to client";
            std::cout.flush();
        #endif


        #ifndef NDEBUG
            std::cout << "\nMPI SERVER: Sending MPI Header";
            std::cout.flush();

            for (int i=0; i<structSize; i++) {
                std::cout <<  "\n MPI Sent DS : Size : " << argList[i].sizeOfArg << "  Type" << argList[i].typeofArg ;
                std::cout.flush();
            }
        #endif
        MPI_Send(argList, structSize, ArgListType, mpi_server_rank, mpi_server_tag, client);

        #ifndef NDEBUG
            std::cout << "\nMPI SERVER: Sent MPI Header";
            std::cout.flush();

            std::cout << "\nMPI SERVER: Sending data";
            std::cout.flush();
        #endif

        //Start sending individual arrrays
        for (const auto& indexOfPtr : indexesOfPointersInArgs) {
            auto paramType = calledFunction->getFunctionType()->getParamType(indexOfPtr);
            while (paramType->getTypeID() == llvm::Type::ArrayTyID || paramType->getTypeID() == llvm::Type::PointerTyID)
            paramType = llvm::cast<llvm::SequentialType>(paramType)->getElementType();

            if (paramType->getTypeID() == llvm::Type::IntegerTyID) {
            MPI_Send(args[indexOfPtr].PointerVal,argList[indexOfPtr].sizeOfArg, MPI_INT, mpi_server_rank, mpi_server_tag, client);
            } else {
            MPI_Send(args[indexOfPtr].PointerVal, argList[indexOfPtr].sizeOfArg, MPI_DOUBLE, mpi_server_rank, mpi_server_tag, client);
            }
            free(args[indexOfPtr].PointerVal);
        }

        #ifndef TIMING 
            auto EndTime = std::chrono::high_resolution_clock::now();
            std::cout << "\n SERVR: MPI_DATA_TRANSFER S->C = " <<    std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime).count() << "\n";
        #endif
    
        #ifndef NDEBUG
            std::cout << "\nMPI SERVER: Data sent";
            std::cout.flush();

            std::cout << "\nMPI SERVER: Return Messages sent";
            std::cout.flush();
        #endif
        
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

        //Send the message
        MPI_Send(msg_buffer.get(), strlen(msg_buffer.get()), MPI_CHAR, mpi_server_rank, mpi_server_tag, client);
    
        MPI_Type_free(&ArgListType);
    
        // TODO: check this!
        MPI_Finalize();
    }
}
