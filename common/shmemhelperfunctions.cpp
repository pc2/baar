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

#include "shmemhelperfunctions.h"

static void error(const char *msg) {
    perror(msg);
    exit(1);
}

void ShmemHelperFunctions::marshallArrayOfSizeAndTypeIntoMemory(void *arr, int64_t size, std::pair<llvm::Type::TypeID, unsigned> typeIDWithBitwidthPointedTo, void *&shmempos)
{
    std::cout << "DEBUG" << ": marshalling array of type " << typeIDWithBitwidthPointedTo.first << ", size " << size << "\n";

    // marshal number of elements in array (total)
    *(int64_t *)shmempos = size;
    shmempos = (int64_t *) shmempos + 1;

    // marshal elements
    switch (typeIDWithBitwidthPointedTo.first) {
        case llvm::Type::FloatTyID:
            for (int i = 0; i < size; i++) {
                *(float *)shmempos = *(float*)arr;
                shmempos = (float *) shmempos + 1;
                arr = (float*) arr + 1;
            }
        case llvm::Type::DoubleTyID:
            for (int i = 0; i < size; i++) {
                *(double *)shmempos = *(double*)arr;
                shmempos = (double *) shmempos + 1;
                arr = (double*) arr + 1;
            }
            break;
        case llvm::Type::X86_FP80TyID:
        case llvm::Type::FP128TyID:
            for (int i = 0; i < size; i++) {
                *(long double *)shmempos = *(long double*)arr;
                shmempos = (long double *) shmempos + 1;
                arr = (long double*) arr + 1;
            }
            break;
        case llvm::Type::IntegerTyID: // Note: LLVM does not differentiate between signed/unsiged int types
            switch (typeIDWithBitwidthPointedTo.second) {
                case 8:
                    for (int i = 0; i < size; i++) {
                        *(int8_t *)shmempos = *(int8_t*)arr;
                        shmempos = (int8_t *) shmempos + 1;
                        arr = (int8_t*) arr + 1;
                    }
                    break;
                case 16:
                    for (int i = 0; i < size; i++) {
                        *(int16_t *)shmempos = *(int16_t*)arr;
                        shmempos = (int16_t *) shmempos + 1;
                        arr = (int16_t*) arr + 1;
                    }
                    break;
                case 32:
                    for (int i = 0; i < size; i++) {
                        *(int32_t *)shmempos = *(int32_t*)arr;
                        shmempos = (int32_t *) shmempos + 1;
                        arr = (int32_t*) arr + 1;
                    }
                    break;
                case 64:
                    for (int i = 0; i < size; i++) {
                        *(int64_t *)shmempos = *(int64_t*)arr;
                        shmempos = (int64_t *) shmempos + 1;
                        arr = (int64_t*) arr + 1;
                    }
                    break;
                default:
                    for (int i = 0; i < size; i++) {
                        *(int *)shmempos = *(int*)arr;
                        shmempos = (int *) shmempos + 1;
                        arr = (int*) arr + 1;
                    }
                    break;
            }
            break;
        default:
            error(std::string("ERROR, LLVM TypeID " + std::to_string((long long)typeIDWithBitwidthPointedTo.first) + " is not supported for arrays").c_str());
    }
}


void ShmemHelperFunctions::unmarshalArrayFromMemoryAsTypeIntoExistingMemory(void *&shmempos, std::pair<llvm::Type::TypeID, unsigned> typeIDAndBitwidthPointedTo, void *pointerToMemory)
{
    int64_t arraySize = *(int64_t *)shmempos;
    shmempos = (int64_t *) shmempos + 1;

    switch (typeIDAndBitwidthPointedTo.first) {
    case llvm::Type::FloatTyID:
        for (int i = 0; i != arraySize; i++) {
            ((float*)pointerToMemory)[i] = *(float *)shmempos;
            shmempos = (float *) shmempos + 1;
        }
        break;
    case llvm::Type::DoubleTyID:
        for (int i = 0; i != arraySize; i++) {
            ((double*)pointerToMemory)[i] = *(double *)shmempos;
            shmempos = (double *) shmempos + 1;
        }
        break;
    case llvm::Type::X86_FP80TyID:
    case llvm::Type::FP128TyID:
        for (int i = 0; i != arraySize; i++) {
            ((long double*)pointerToMemory)[i] = *(long double *)shmempos;
            shmempos = (long double *) shmempos + 1;
        }
        break;
    case llvm::Type::IntegerTyID:
        switch(typeIDAndBitwidthPointedTo.second) {
        case 8:
            for (int i = 0; i != arraySize; i++) {
                ((int8_t*)pointerToMemory)[i] = *(int8_t *)shmempos;
                shmempos = (int8_t *) shmempos + 1;
            }
            break;
        case 16:
            for (int i = 0; i != arraySize; i++) {
                ((int16_t*)pointerToMemory)[i] = *(int16_t *)shmempos;
                shmempos = (int16_t *) shmempos + 1;
            }
            break;
        case 32:
            for (int i = 0; i != arraySize; i++) {
                ((int32_t*)pointerToMemory)[i] = *(int32_t *)shmempos;
                shmempos = (int32_t *) shmempos + 1;
            }
            break;
        case 64:
            for (int i = 0; i != arraySize; i++) {
                ((int64_t*)pointerToMemory)[i] = *(int64_t *)shmempos;
                shmempos = (int64_t *) shmempos + 1;
            }
            break;
        default:
            for (int i = 0; i != arraySize; i++) {
                ((int*)pointerToMemory)[i] = *(int *)shmempos;
                shmempos = (int *) shmempos + 1;
            }
        }
        break;
    default:
        std::cerr << "LLVM TypeID " << typeIDAndBitwidthPointedTo.first << " is not supported as array element.\n";
        exit(1);
    }

    return;
}


void *ShmemHelperFunctions::unmarshalArrayFromMemoryAsTypeIntoNewMemory(void *&shmempos, llvm::Type *typePointedTo)
{
    int64_t arraySize = *(int64_t *)shmempos;
    auto pointedToTypeID = typePointedTo->getTypeID();

    unsigned pointedToBitwidth;
    if (pointedToTypeID == llvm::Type::IntegerTyID)
        pointedToBitwidth = llvm::cast<llvm::IntegerType>(typePointedTo)->getBitWidth();

    void* array;
    switch (pointedToTypeID) {
    case llvm::Type::FloatTyID:
        array = malloc(arraySize*sizeof(float));
        break;
    case llvm::Type::DoubleTyID:
        array = malloc(arraySize*sizeof(double));
        break;
    case llvm::Type::X86_FP80TyID:
    case llvm::Type::FP128TyID:
        array = malloc(arraySize*sizeof(long double));
        break;
    case llvm::Type::IntegerTyID:
        switch(pointedToBitwidth) {
        case 8:
            array = malloc(arraySize*sizeof(int8_t));
            break;
        case 16:
            array = malloc(arraySize*sizeof(int16_t));
            break;
        case 32:
            array = malloc(arraySize*sizeof(int32_t));
            break;
        case 64:
            array = malloc(arraySize*sizeof(int64_t));
            break;
        default:
            array = malloc(arraySize*sizeof(int));
        }
        break;
    default:
        std::cerr << "LLVM TypeID " << pointedToTypeID << " is not supported as array element.\n";
        exit(1);
    }

    unmarshalArrayFromMemoryAsTypeIntoExistingMemory(shmempos, std::pair<llvm::Type::TypeID, unsigned>(pointedToTypeID, pointedToBitwidth), array);

    return array;
}
