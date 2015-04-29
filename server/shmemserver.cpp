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

#include "shmemserver.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstdint>
#include <iostream>
#include <cstdlib>

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/ExecutionEngine/GenericValue.h"

#include "../common/shmemhelperfunctions.h"

ShmemServer::ShmemServer(backendTypes backendType) : AbstractServer(backendType)
{
}

void ShmemServer::initCommunication()
{
#ifndef NDEBUG
    std::cout << "Creating shared memory region of size: " << (SHMEM_SIZE/1024)/1024 << " MB" << std::endl;
#endif

    int shmemfd = shm_open(SHMEM_NAME.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (shmemfd == -1)
        error("ERROR, could not allocate shared memory");

    if (ftruncate(shmemfd, SHMEM_SIZE) != 0)
        error("ERROR, unable to resize shared memory region");

    shmemptr = mmap(0, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmemfd, 0);
    if (shmemptr == MAP_FAILED)
        error("ERROR, unable to map shared memory region into memory");
    close(shmemfd);
    *(char *) shmemptr = '\0';

    // create and initialise semaphore for synchronisation
    if (sem_unlink(SHMEM_SEM_NAME.c_str()) == 0)
        std::cout << "WARNING" << ": server did not exit cleanly previously\n";
    shmem_sem = sem_open(SHMEM_SEM_NAME.c_str(), O_CREAT, 0666, 0);
    if (shmem_sem == SEM_FAILED)
        error("ERROR, unable to create semaphore");

    // create and initialise semaphore for forcing order on synchronisation
    if (sem_unlink(SHMEM_FORCE_ORDER_SEM_NAME.c_str()) == 0)
        std::cout << "WARNING" << ": server did not exit cleanly previously\n";
    shmem_force_order_sem = sem_open(SHMEM_FORCE_ORDER_SEM_NAME.c_str(), O_CREAT, 0666, 1);
    if (shmem_force_order_sem == SEM_FAILED)
        error("ERROR, unable to create semaphore");
}

void ShmemServer::handleCommunication()
{
    sem_wait(shmem_force_order_sem);
    // wait for client to connect
    sem_wait(shmem_sem);
    sem_post(shmem_force_order_sem);
    handle_conn();
}

void ShmemServer::cleanupCommunication()
{
    // close and destroy shared semaphore
    if (sem_close(shmem_force_order_sem) == -1)
        perror("ERROR, unable to close semaphore");

    if (sem_unlink(SHMEM_FORCE_ORDER_SEM_NAME.c_str()) == -1)
        perror("ERROR, unable to unlink semaphore");

    // close and destroy shared semaphore
    if (sem_close(shmem_sem) == -1)
        perror("ERROR, unable to close semaphore");

    if (sem_unlink(SHMEM_SEM_NAME.c_str()) == -1)
        perror("ERROR, unable to unlink semaphore");

#ifndef NDEBUG
    std::cout << "Clearing shared memory" << std::endl;
#endif

    // make completely sure that the exit symbol is gone, in case of an immediate rerun of the server
    *(char *) shmemptr = '\0';

    if (munmap(shmemptr, SHMEM_SIZE))
        perror("ERROR, unable to unmap shared memory region from memory");

    if (shm_unlink(SHMEM_NAME.c_str()))
        perror("ERROR, unable to unlink shared memory region");
}

void ShmemServer::unmarshalCallArgs(char *buffer, int functionName_offset, llvm::Function *calledFunction, std::vector<llvm::GenericValue> &args, std::list<std::vector<llvm::GenericValue>::size_type> &indexesOfPointersInArgs)
{
    llvm::FunctionType *CalledFuncType = calledFunction->getFunctionType();
    int numArgs = CalledFuncType->getNumParams();

    // skip function name in msg from client
    void *shmempos = buffer + functionName_offset + 1;

    // parse arguments into GenericValues for the ExecutionEngine
    for (int i = 0; i < numArgs; i++) {
        llvm::GenericValue CurrArg;
        llvm::Type *CurrType = CalledFuncType->getParamType(i);
        llvm::Type* pointerToTy;
        switch (CurrType->getTypeID()) {
            case llvm::Type::PointerTyID:
                    pointerToTy = static_cast<llvm::PointerType*>(CurrType)->getElementType();
                    while (pointerToTy->getTypeID() == llvm::Type::ArrayTyID || pointerToTy->getTypeID() == llvm::Type::PointerTyID)
                        pointerToTy = llvm::cast<llvm::SequentialType>(pointerToTy)->getElementType();

                    CurrArg.PointerVal = ShmemHelperFunctions::unmarshalArrayFromMemoryAsTypeIntoNewMemory(shmempos, pointerToTy);
                    indexesOfPointersInArgs.push_back(i);
                break;
            case llvm::Type::FloatTyID:
                CurrArg.FloatVal = *(float *)shmempos;
                shmempos = (float *) shmempos + 1;
                break;
            case llvm::Type::DoubleTyID:
                CurrArg.DoubleVal = *(double *)shmempos;
                shmempos = (double *) shmempos + 1;
                break;
            case llvm::Type::X86_FP80TyID: {
                // easiest way to get correct APInt representing this long double
                char valstring[16];
                sprintf(valstring, "%La", *(long double *)shmempos);
                shmempos = (long double *) shmempos + 1;
                llvm::APFloat bigval(llvm::APFloat::x87DoubleExtended, valstring);
                CurrArg.IntVal = bigval.bitcastToAPInt();
                break;
            }
            case llvm::Type::FP128TyID: {
                 // easiest way to get correct APInt representing this long double
                char valstring[16];
                sprintf(valstring, "%La", *(long double *)shmempos);
                shmempos = (long double *) shmempos + 1;
                llvm::APFloat bigval(llvm::APFloat::IEEEquad, valstring);
                CurrArg.IntVal = bigval.bitcastToAPInt();
                break;
            }
            case llvm::Type::IntegerTyID: { // Note: LLVM does not differentiate between signed/unsiged int types
                auto bitwidth = ((llvm::IntegerType*)CurrType)->getBitWidth();
                uint64_t intval;
                switch (bitwidth) {
                case 32:
                    intval = *(int32_t *)shmempos;
                    shmempos = (int32_t *) shmempos + 1;
                    break;
                case 64:
                    intval = *(int64_t *)shmempos;
                    shmempos = (int64_t *) shmempos + 1;
                    break;
                default:
                    intval = *(int *)shmempos;
                    shmempos = (int *) shmempos + 1;
                    break;
                }
                llvm::APInt apintval(bitwidth, intval);
                CurrArg.IntVal = apintval;
                break;
            }
            default:
                std::cerr << "LLVM TypeID " << CurrType->getTypeID() << " of argument " << i << " is not supported.\n";
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
        std::cout << ")'\n";
}

void ShmemServer::handle_conn()
{
#ifndef NDEBUG
    std::cout << "DEBUG" << ": got IR" << std::endl;
#endif
    // initialize backend with received IR
    auto backend = parseIRtoBackend((char *) shmemptr);
    *(long *)shmemptr = TimeDiffOpt.count();
    *(((long *)shmemptr) + 1) = TimeDiffInit.count();
    // signal to client: got IR, ready to get calls, time measures in shmem
    sem_post(shmem_sem);

    while (1) {
        sem_wait(shmem_force_order_sem);
        // wait for call
        sem_wait(shmem_sem);
        sem_post(shmem_force_order_sem);
#ifndef NDEBUG
        std::cout << "DEBUG" << ": got call \n";
#endif
        // check for exit symbol
        if (((char *)shmemptr)[0] == ';') {
#ifndef NDEBUG
            std::cout << "DEBUG" << ": found exit symbol \n";
#endif
            break;
        }

        llvm::Function* calledFunction = nullptr;
        std::vector<llvm::GenericValue> args;
        std::list<std::vector<llvm::GenericValue>::size_type> indexesOfPointersInArgs;
        llvm::GenericValue result = handleCall(backend.get(), (char *) shmemptr, calledFunction, args, indexesOfPointersInArgs);

        auto shmempos = shmemptr;
        // write measured time to memory
        *(long *)shmempos = TimeDiffLastExecution.count();
        shmempos = (long *)shmempos + 1;

        // write changes to args and result back to shared memory
        for (const auto& indexOfPtr : indexesOfPointersInArgs) {
            auto paramType = calledFunction->getFunctionType()->getParamType(indexOfPtr);
            while (paramType->getTypeID() == llvm::Type::ArrayTyID || paramType->getTypeID() == llvm::Type::PointerTyID)
                paramType = llvm::cast<llvm::SequentialType>(paramType)->getElementType();

            if (paramType->getTypeID() == llvm::Type::IntegerTyID) {
                ShmemHelperFunctions::marshallArrayOfSizeAndTypeIntoMemory(args[indexOfPtr].PointerVal, args[indexOfPtr+1].IntVal.getSExtValue(), std::pair<llvm::Type::TypeID, unsigned>(paramType->getTypeID(), ((llvm::IntegerType*)paramType)->getBitWidth()), shmempos);
            } else
                ShmemHelperFunctions::marshallArrayOfSizeAndTypeIntoMemory(args[indexOfPtr].PointerVal, args[indexOfPtr+1].IntVal.getSExtValue(), std::pair<llvm::Type::TypeID, unsigned>(paramType->getTypeID(), 0U), shmempos);

            free(args[indexOfPtr].PointerVal);
        }

        switch (calledFunction->getReturnType()->getTypeID()) {
            case llvm::Type::VoidTyID:
                // void return
                break;
            case llvm::Type::FloatTyID:
                *(float *)shmempos  = result.FloatVal;
                break;
            case llvm::Type::DoubleTyID:
                *(double *)shmempos = result.DoubleVal;
                break;
            case llvm::Type::X86_FP80TyID: {
                char tmpHexString[64];
                llvm::APFloat(llvm::APFloat::x87DoubleExtended, result.IntVal).convertToHexString(tmpHexString, 0U, false, llvm::APFloat::roundingMode::rmNearestTiesToEven);
                *(long double *)shmempos = strtold(tmpHexString, nullptr);
                break;
            }
            case llvm::Type::FP128TyID: {
                char tmpHexString[64];
                llvm::APFloat(llvm::APFloat::IEEEquad, result.IntVal).convertToHexString(tmpHexString, 0U, false, llvm::APFloat::roundingMode::rmNearestTiesToEven);
                *(long double *)shmempos = strtold(tmpHexString, nullptr);
                break;
             }
            case llvm::Type::IntegerTyID: // Note: LLVM does not differentiate between signed/unsiged int types
                switch (result.IntVal.getBitWidth()) {
                    case 8:
                        *(uint8_t *)shmempos = (uint8_t) result.IntVal.getZExtValue();
                        break;
                    case 16:
                        *(uint16_t *)shmempos = (uint16_t) result.IntVal.getZExtValue();
                        break;
                    case 32:
                        *(uint32_t *)shmempos = (uint32_t) result.IntVal.getZExtValue();
                        break;
                    case 64:
                        *(uint64_t *)shmempos = (uint64_t) result.IntVal.getZExtValue();
                        break;
                    default:
                        error(std::string("ERROR, integer bitwidth of " + std::to_string(result.IntVal.getBitWidth()) + " not supported").c_str());
                 }
                break;
            default:
            error(std::string("ERROR, LLVM TypeID " + std::to_string(calledFunction->getReturnType()->getTypeID()) + " of result of function \"" + calledFunction->getName().str() + "\" is not supported").c_str());
        }

#ifndef NDEBUG
        std::cout << "DEBUG" << ": signaling 'result is ready' to client \n";
#endif

        sem_post(shmem_sem);
    }
}
