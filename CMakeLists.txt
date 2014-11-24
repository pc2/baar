cmake_minimum_required(VERSION 2.8)

project(BAAR)

if(NOT LLVM_SRC_DIR)
    message(FATAL_ERROR "LLVM_SRC_DIR undefined, please define it to point to LLVM source")
endif()

if(NOT LLVM_BIN_DIR)
    message(FATAL_ERROR "LLVM_BIN_DIR undefined, please define it to point to the LLVM build corresponding to the sources in LLVM_SRC_DIR")
endif()

set(CMAKE_BUILD_TYPE Debug)
add_definitions(-D_DEBUG -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)

link_directories(${LLVM_BIN_DIR}/lib)
include_directories(${LLVM_SRC_DIR}/include ${LLVM_BIN_DIR}/include)

find_package(Threads REQUIRED)
find_package(OpenMP)
find_package(Curses)

add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)
