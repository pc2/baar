cmake_minimum_required(VERSION 2.8)

project(BAAR)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "k1om")

	SET(CMAKE_SYSTEM_NAME Linux)
	SET(CMAKE_SYSTEM_PROCESSOR k1om)
	SET(CMAKE_SYSTEM_VERSION 1)

	SET(_CMAKE_TOOLCHAIN_PREFIX x86_64-k1om-linux-)

	SET(CMAKE_FIND_ROOT_PATH /usr/linux-k1om-4.7)
endif()


set(CMAKE_C_COMPILER "mpiicc")
set(CMAKE_CXX_COMPILER "mpiicpc")


if(NOT LLVM_SRC_DIR)
    message(FATAL_ERROR "LLVM_SRC_DIR undefined, please define it to point to LLVM source")
endif()

if(NOT LLVM_BIN_DIR)
    message(FATAL_ERROR "LLVM_BIN_DIR undefined, please define it to point to the LLVM build corresponding to the sources in LLVM_SRC_DIR")
endif()

set(CMAKE_BUILD_TYPE Debug)

add_definitions(-DNDEBUG=1)
add_definitions(-D_NDEBUG=1 -D_DEBUG -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)

link_directories(${LLVM_BIN_DIR}/lib)
include_directories(${LLVM_SRC_DIR}/include ${LLVM_BIN_DIR}/include)
include_directories(${LLVM_SRC_DIR}/tools/polly/include ${LLVM_BIN_DIR}/tools/polly/include)

find_package(Threads REQUIRED)
find_package(OpenMP)
# find_package(Curses)

add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)
add_subdirectory(utils)
