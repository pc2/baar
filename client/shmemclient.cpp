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

#include "shmemclient.h"

#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <unistd.h>
#include <cstring>
#include <climits>
#include <cstdint>
#include <sys/file.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <iostream>
#include <string>
#include <list>

#include "llvm/IR/DerivedTypes.h"

#include "../consts.h"
#include "../common/shmemhelperfunctions.h"

ShmemClient::ShmemClient() : AbstractClient()
{
}

ShmemClient::~ShmemClient()
{
    // write exit message to server
    strcpy((char *)shmemptr, ";");
    sem_post(shmem_sem);

    // close shared semaphores
    if (sem_close(shmem_force_order_sem) == -1)
        perror("ERROR, unable to close semaphore");
    if (sem_close(shmem_sem) == -1)
        perror("ERROR, unable to close semaphore");

    if (munmap(shmemptr, SHMEM_SIZE))
        perror("ERROR, unable to unmap shared memory region from memory");
}

void ShmemClient::connectToAccelerator()
{
    // open shmem
    int shmemfd = shm_open(SHMEM_NAME.c_str(), O_RDWR, 0666);
    if (shmemfd == -1)
        error("ERROR, could not get shared memory");

    shmemptr = mmap(0, SHMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shmemfd, 0);
    if (shmemptr == MAP_FAILED)
        error("ERROR, unable to map shared memory region into memory");
    close(shmemfd);

    // open shared semaphore for synchronisation
    shmem_sem = sem_open(SHMEM_SEM_NAME.c_str(), 0);
    if (shmem_sem == SEM_FAILED)
        error("ERROR, unable to open semaphore");

    // open shared semaphore for forcing order on synchronisation
    shmem_force_order_sem = sem_open(SHMEM_FORCE_ORDER_SEM_NAME.c_str(), 0);
    if (shmem_force_order_sem == SEM_FAILED)
        error("ERROR, unable to open semaphore");

#ifndef NDEBUG
    int shmem_sem_val, shmem_force_order_sem_val;
    sem_getvalue(shmem_sem, &shmem_sem_val);
    sem_getvalue(shmem_force_order_sem, &shmem_force_order_sem_val);
    std::cout << "DEBUG" << ": shmem_sem(" << shmem_sem_val << ") shmem_force_order_sem(" << shmem_force_order_sem_val << ")\n";
#endif
}

void ShmemClient::writeIR(void *shmemptr, const std::string& IR)
{
   for (int i = 0; i < IR.size()+1; i++)
       ((char*)shmemptr)[i] = IR.c_str()[i];
}

void ShmemClient::marshallCall(const char *funcNameAndArgs, va_list args, std::list<std::pair<void *, std::pair<llvm::Type::TypeID, unsigned> > > &pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate)
{
#ifndef NDEBUG
    std::cout << "DEBUG" << ": marshallCall(\"" << funcNameAndArgs << "\", ...)" << std::endl;
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

    // build call: "functionName:val1val2...valn", vali in machine form
    void *shmempos = shmemptr;
    for (int i = 0; funcName[i] != '\0'; i++) {
        *(char *)shmempos = funcName[i];
        shmempos = (char *)shmempos + 1;
    }
    *(char *)shmempos = ':';
    shmempos = (char *)shmempos + 1;

    int i = 0;
    auto intBWiterator = intBitWidths.begin();
    auto pointersToTyIDIterator = pointersPointedToTypeID.begin();
    llvm::Type::TypeID pointingToTyID = llvm::Type::NumTypeIDs; // = explicitly unset
    unsigned pointingToIntBW;
    void* ptrPending = nullptr;
    for (const auto& currArg : argTypes) {
        i++;
        switch (currArg) {
            case llvm::Type::PointerTyID: {
                pointingToTyID = *pointersToTyIDIterator++;
                pointingToIntBW = *intBWiterator++;
                std::cout << "DEBUG" << ": argument " << i << " is pointer to type with TypeID " << pointingToTyID << ", assuming argument " << i+1 << " is int, giving number of elements in area pointed to\n";
                std::cout << "DEBUG" << ": area will be marshalled with handling of next argument\n";
                ptrPending = va_arg(args, void*);
                auto ptrPointingToType = std::make_pair(pointingToTyID, pointingToIntBW);
                pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate.push_back(std::pair<void *, std::pair<llvm::Type::TypeID, unsigned>>(ptrPending, ptrPointingToType));
                break;
            }
            case llvm::Type::FloatTyID:
                *(float *)shmempos = (float) va_arg(args, double);
                shmempos = (float *) shmempos + 1;
                break;
            case llvm::Type::DoubleTyID:
                *(double *)shmempos = va_arg(args, double);
                shmempos = (double *) shmempos + 1;
                break;
            case llvm::Type::X86_FP80TyID:
            case llvm::Type::FP128TyID:
                *(long double *)shmempos = va_arg(args, long double);
                shmempos = (long double *) shmempos + 1;
                break;
            case llvm::Type::IntegerTyID:  // Note: LLVM does not differentiate between signed/unsiged int types
                switch (*intBWiterator++) {
                    case 32: {
                        int32_t tmp = va_arg(args, int32_t);
                        if (pointingToTyID != llvm::Type::NumTypeIDs) {
                            ShmemHelperFunctions::marshallArrayOfSizeAndTypeIntoMemory(ptrPending, tmp, std::pair<llvm::Type::TypeID, unsigned>(pointingToTyID, pointingToIntBW), shmempos);
                            pointingToTyID = llvm::Type::NumTypeIDs;
                            pointingToIntBW = 0U;
                        }
                        *(int32_t *)shmempos = tmp;
                        shmempos = (int32_t *) shmempos + 1;
                        break;
                    }
                    case 64: {
                        int64_t tmp = va_arg(args, int64_t);
                        if (pointingToTyID != llvm::Type::NumTypeIDs) {
                            ShmemHelperFunctions::marshallArrayOfSizeAndTypeIntoMemory(ptrPending, tmp, std::pair<llvm::Type::TypeID, unsigned>(pointingToTyID, pointingToIntBW), shmempos);
                            pointingToTyID = llvm::Type::NumTypeIDs;
                            pointingToIntBW = 0U;
                        }
                        *(int64_t *)shmempos = tmp;
                        shmempos = (int64_t *) shmempos + 1;
                        break;
                    }
                    default: {
                        int tmp = va_arg(args, int);
                        if (pointingToTyID != llvm::Type::NumTypeIDs) {
                            ShmemHelperFunctions::marshallArrayOfSizeAndTypeIntoMemory(ptrPending, tmp, std::pair<llvm::Type::TypeID, unsigned>(pointingToTyID, pointingToIntBW), shmempos);
                            pointingToTyID = llvm::Type::NumTypeIDs;
                            pointingToIntBW = 0U;
                        }
                        *(int *)shmempos = tmp;
                        shmempos = (int *) shmempos + 1;
                        break;
                    }
                }
                break;
            default:
                error(std::string("ERROR, LLVM TypeID " + std::to_string(currArg) + " of argument " + std::to_string(i) + " in function \"" + funcName + "\" is not supported").c_str());
        }
    }
}

void ShmemClient::initialiseAccelerationWithIR(const std::string &IR)
{
    connectToAccelerator();

    // send IR to server
    writeIR(shmemptr, IR);

    // signal to server: connection and IR ready
    sem_post(shmem_sem);

    sem_wait(shmem_force_order_sem);
    // wait for server to be ready for calls
    sem_wait(shmem_sem);
    // get time measurements
    TimeDiffOpt = *(long *)shmemptr;
    TimeDiffInit = *(((long *)shmemptr + 1));
    sem_post(shmem_force_order_sem);
}

void ShmemClient::callAcc(const char *retTypeFuncNameArgTypes, va_list args)
{
    assert(shmemptr != nullptr && "Connection to server uninitialised!");

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
    marshallCall(behindResTypePtr, args, pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate);
    // signal to server: call ready
    sem_post(shmem_sem);

    sem_wait(shmem_force_order_sem);
    // wait for result ready, read mesured time, changes to arguments and result from shmem afterwards
    sem_wait(shmem_sem);
    sem_post(shmem_force_order_sem);

    auto shmempos = shmemptr;
    TimeDiffLastExecution = *(long *)shmempos;
    shmempos = (long *)shmempos + 1;

    // unmarshal changes to pointed to memory areas
    std::cout << "DEBUG" << ": updating " << pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate.size() << " arguments\n";
    for (const auto& pointerAndType : pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate)
        ShmemHelperFunctions::unmarshalArrayFromMemoryAsTypeIntoExistingMemory(shmempos, pointerAndType.second, pointerAndType.first);

    // interpret result with typeinfo from resType
    switch (retType) {
        case llvm::Type::VoidTyID:
            std::cout << "DEBUG" << ": got void return\n";
            break;
        case llvm::Type::FloatTyID:
            *(float*)retContainerPtr = *(float*) shmempos;
        case llvm::Type::DoubleTyID:
            *(double*)retContainerPtr = *(double*) shmempos;
            break;
        case llvm::Type::X86_FP80TyID:
        case llvm::Type::FP128TyID:
            *(long double*)retContainerPtr = *(long double*) shmempos;
            break;
        case llvm::Type::IntegerTyID: // Note: LLVM does not differentiate between signed/unsiged int types
            switch (retTypeBitWidth) {
                case 8:
                    *(uint8_t*)retContainerPtr = *(uint8_t*) shmempos;
                    break;
                case 16:
                    *(uint16_t*)retContainerPtr = *(uint16_t*) shmempos;
                    break;
                case 32:
                    *(uint32_t*)retContainerPtr = *(uint32_t*) shmempos;
                    break;
                case 64:
                    *(uint64_t*)retContainerPtr = *(uint64_t*) shmempos;
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
