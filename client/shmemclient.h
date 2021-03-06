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

#ifndef SHMEMCLIENT_H
#define SHMEMCLIENT_H

#include "abstractclient.h"

#include <string>
#include <list>

#include <semaphore.h>

#include "llvm/IR/DerivedTypes.h"

class ShmemClient : public AbstractClient
{
    void *shmemptr = nullptr;
    sem_t *shmem_sem = nullptr;
    sem_t *shmem_force_order_sem = nullptr;

    void connectToAccelerator();
    void writeIR(void *shmemptr, const std::string &IR);
    void marshallCall(const char *funcNameAndArgs, va_list args, std::list<std::pair<void *, std::pair<llvm::Type::TypeID, unsigned> > > &pointersAndTypeIDWithBitwidthPointedToAwaitingUpdate);

public:
    ShmemClient();
    virtual ~ShmemClient();
    virtual void initialiseAccelerationWithIR(const std::string &IR);
    virtual void callAcc(const char *retTypeFuncNameArgTypes, va_list args);
};

#endif // SHMEMCLIENT_H
