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

#include "sockethelperfunctions.h"

static void error(const char *msg) {
    perror(msg);
    exit(1);
}

void SocketHelperFunctions::marshallArrayOfSizeAndTypeIntoBuffer(void* arr, uint64_t size, std::pair<llvm::Type::TypeID, unsigned> typeIDAndBitwidthPointedTo, char* outputBuffer) {
    std::cout << "DEBUG" << ": marshalling array of type " << typeIDAndBitwidthPointedTo.first << ((typeIDAndBitwidthPointedTo.second) ? ":" + std::to_string((unsigned long long)typeIDAndBitwidthPointedTo.second) : "") << ", size " << size << std::endl;
    unsigned arrayStrLen = 0U;
    std::shared_ptr<char> arrayStr((char*)calloc(MAX_ARR_SIZE, sizeof(char)), &free);
    char max_element[100];
    sprintf(max_element, ";%La", std::numeric_limits<long double>::max());
    const auto max_element_size = strlen(max_element) + 1;

    // marshal number of elements in array (total)
    arrayStrLen += sprintf(arrayStr.get(), ":%" PRIu64, size);

    // marshal elements
    switch (typeIDAndBitwidthPointedTo.first) {
        case llvm::Type::FloatTyID:
            for (uint64_t i = 0; i < size; i++) {
                if (arrayStrLen < MAX_ARR_SIZE - max_element_size)
                    arrayStrLen += sprintf(arrayStr.get() + arrayStrLen, ";%la", *((float*)arr));
                else
                    error("ERROR, array in marshalled form is bigger than array buffer");
                arr = (float*)arr + 1;
            }
        case llvm::Type::DoubleTyID:
            for (uint64_t i = 0; i < size; i++) {
                if (arrayStrLen < MAX_ARR_SIZE - max_element_size)
                    arrayStrLen += sprintf(arrayStr.get() + arrayStrLen, ";%la", *((double*)arr));
                else
                    error("ERROR, array in marshalled form is bigger than array buffer");
                arr = (double*)arr + 1;
            }
            break;
        case llvm::Type::X86_FP80TyID:
        case llvm::Type::FP128TyID:
            for (uint64_t i = 0; i < size; i++) {
                if (arrayStrLen < MAX_ARR_SIZE - max_element_size)
                    arrayStrLen += sprintf(arrayStr.get() + arrayStrLen, ";%La", *((long double*)arr));
                else
                    error("ERROR, array in marshalled form is bigger than array buffer");
                arr = (long double*)arr + 1;
            }
            break;
        case llvm::Type::IntegerTyID: // Note: LLVM does not differentiate between signed/unsiged int types
            switch (typeIDAndBitwidthPointedTo.second) {
                case 8:
                    for (uint64_t i = 0; i < size; i++) {
                        if (arrayStrLen < MAX_ARR_SIZE - max_element_size)
                            arrayStrLen += sprintf(arrayStr.get() + arrayStrLen, ";%" PRIx8, *((int8_t*)arr));
                        else
                            error("ERROR, array in marshalled form is bigger than array buffer");
                        arr = (int8_t*) arr + 1;
                    }
                    break;
                case 16:
                    for (uint64_t i = 0; i < size; i++) {
                        if (arrayStrLen < MAX_ARR_SIZE - max_element_size)
                            arrayStrLen += sprintf(arrayStr.get() + arrayStrLen, ";%" PRIx16, *((int16_t*)arr));
                        else
                            error("ERROR, array in marshalled form is bigger than array buffer");
                        arr = (int16_t*) arr + 1;
                    }
                    break;
                case 32:
                    for (uint64_t i = 0; i < size; i++) {
                        if (arrayStrLen < MAX_ARR_SIZE - max_element_size)
                            arrayStrLen += sprintf(arrayStr.get() + arrayStrLen, ";%" PRIx32, *((int32_t*)arr));
                        else
                            error("ERROR, array in marshalled form is bigger than array buffer");
                        arr = (int32_t*) arr + 1;
                    }
                    break;
                case 64:
                    for (uint64_t i = 0; i < size; i++) {
                        if (arrayStrLen < MAX_ARR_SIZE - max_element_size)
                            arrayStrLen += sprintf(arrayStr.get() + arrayStrLen, ";%" PRIx64, *((int64_t*)arr));
                        else
                            error("ERROR, array in marshalled form is bigger than array buffer");
                        arr = (int64_t*) arr + 1;
                    }
                    break;
                default:
                    for (uint64_t i = 0; i < size; i++) {
                        if (arrayStrLen < MAX_ARR_SIZE - max_element_size)
                            arrayStrLen += sprintf(arrayStr.get() + arrayStrLen, ";%x", *((int*)arr));
                        else
                            error("ERROR, array in marshalled form is bigger than array buffer");
                        arr = (int*) arr + 1;
                    }
                    break;
            }
            break;
        default:
            error(std::string("ERROR, LLVM TypeID " + std::to_string((long long)typeIDAndBitwidthPointedTo.first) + " is not supported for arrays").c_str());
    }
    strcat(outputBuffer, arrayStr.get());
}

void SocketHelperFunctions::unmarshalArrayFromBufferAsTypeIntoExistingMemory(char *&buffer, std::pair<llvm::Type::TypeID, unsigned> typeIDAndBitwidthPointedTo, void* pointerToMemory) {
    int64_t numberOfElements = strtoll(buffer, &buffer, 10);
    buffer++;

    switch (typeIDAndBitwidthPointedTo.first) {
    case llvm::Type::FloatTyID:
        for (int i = 0; i != numberOfElements; i++) {
            ((float*)pointerToMemory)[i] = strtod(buffer, &buffer);
            buffer++;
        }
        break;
    case llvm::Type::DoubleTyID:
        for (int i = 0; i != numberOfElements; i++) {
            ((double*)pointerToMemory)[i] = strtod(buffer, &buffer);
            buffer++;
        }
        break;
    case llvm::Type::X86_FP80TyID:
    case llvm::Type::FP128TyID:
        for (int i = 0; i != numberOfElements; i++) {
            ((long double*)pointerToMemory)[i] = strtold(buffer, &buffer);
            buffer++;
        }
        break;
    case llvm::Type::IntegerTyID:
        switch(typeIDAndBitwidthPointedTo.second) {
        case 8:
            for (int i = 0; i != numberOfElements; i++) {
                ((int8_t*)pointerToMemory)[i] = strtol(buffer, &buffer, 16);
                buffer++;
            }
            break;
        case 16:
            for (int i = 0; i != numberOfElements; i++) {
                ((int16_t*)pointerToMemory)[i] = strtol(buffer, &buffer, 16);
                buffer++;
            }
            break;
        case 32:
            for (int i = 0; i != numberOfElements; i++) {
                ((int32_t*)pointerToMemory)[i] = strtol(buffer, &buffer, 16);
                buffer++;
            }
            break;
        case 64:
            for (int i = 0; i != numberOfElements; i++) {
                ((int64_t*)pointerToMemory)[i] = strtoll(buffer, &buffer, 16);
                buffer++;
            }
            break;
        default:
            for (int i = 0; i != numberOfElements; i++) {
                ((int*)pointerToMemory)[i] = strtol(buffer, &buffer, 16);
                buffer++;
            }
        }
        break;
    default:
        std::cerr << "LLVM TypeID " << typeIDAndBitwidthPointedTo.first << " is not supported as array element.\n";
        exit(1);
    }
    buffer--;

    return;
}


void *SocketHelperFunctions::unmarshalArrayFromBufferAsTypeIntoNewMemory(char *&buffer, llvm::Type *typePointedTo)
{
    int64_t arraySize = strtoll(buffer, nullptr, 10);
    auto pointedToTypeID = typePointedTo->getTypeID();

    unsigned pointedToTypeBitwidth;
    if (pointedToTypeID == llvm::Type::IntegerTyID)
        pointedToTypeBitwidth = llvm::cast<llvm::IntegerType>(typePointedTo)->getBitWidth();

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
        switch(pointedToTypeBitwidth) {
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

    unmarshalArrayFromBufferAsTypeIntoExistingMemory(buffer, std::pair<llvm::Type::TypeID, unsigned>(pointedToTypeID, pointedToTypeBitwidth), array);

    return array;
}
