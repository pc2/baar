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

#ifndef CONSTS_H
#define CONSTS_H

#include <string>
#include <unistd.h>
#include <climits>

// shmem constants
const std::string SHMEM_NAME = "/RPCAcc_shmem";
const std::string SHMEM_SEM_NAME = "/RPCAcc_shmem_sem";
const std::string SHMEM_FORCE_ORDER_SEM_NAME = "/RPCAcc_shmem_force_order_sem";
const size_t SHMEM_SIZE = sysconf(_SC_PAGE_SIZE) << 17; // 512 MB on Linux 64

// socket constants
constexpr unsigned short SERVER_PORT = 55055;
constexpr unsigned MSG_BUFFER_SIZE = UINT_MAX >> 3;
constexpr unsigned IR_BUFFER_SIZE = UINT_MAX >> 12;
constexpr unsigned short MAX_VAL_SIZE = USHRT_MAX;
constexpr unsigned MAX_ARR_SIZE = UINT_MAX >> 3;

#endif // CONSTS_H
