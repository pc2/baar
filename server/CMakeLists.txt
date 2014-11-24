add_subdirectory(pass)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

add_executable(baar_server main.cpp abstractserver.cpp shmemserver.cpp socketserver.cpp abstractbackend.cpp jitbackend.cpp interpreterbackend.cpp extcompilerbackend.cpp llvm_ffi.cpp)

target_link_libraries(baar_server baar_common cbackend)

find_library(LLVMX86_FOUND LLVMX86Utils PATHS ${LLVM_BIN_DIR}/lib)
if(LLVMX86_FOUND)
    target_link_libraries(baar_server
        LLVMX86Disassembler
        LLVMX86AsmParser
        LLVMX86CodeGen
        LLVMX86Desc
        LLVMX86Info
        LLVMX86AsmPrinter
        LLVMX86Utils
    )
endif()

target_link_libraries(baar_server
        LLVMPolly
	LLVMIRReader 
	LLVMBitReader 
	LLVMAsmParser 
	LLVMSelectionDAG 
	LLVMAsmPrinter 
	LLVMMCParser 
	LLVMJIT 
        LLVMRuntimeDyld
        LLVMInterpreter
	LLVMExecutionEngine 
	LLVMCodeGen
	LLVMipo
	LLVMVectorize
	LLVMObjCARCOpts 
	LLVMScalarOpts 
	LLVMInstCombine
	LLVMInstrumentation
	LLVMTransformUtils 
	LLVMipa	
	LLVMAnalysis 
	LLVMTarget 
	LLVMMC 
	LLVMObject 
	LLVMCore 
	LLVMSupport
)

target_link_libraries(baar_server ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

message(STATUS "CMAKE_SYSTEM_PROCESSOR is ${CMAKE_SYSTEM_PROCESSOR}")
if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "k1om")
    target_link_libraries(baar_server ${CMAKE_SOURCE_DIR}/libffi.a)
    add_definitions(-D_K1OM_)
else()
    target_link_libraries(baar_server ffi)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(baar_server rt)
endif()

if(CURSES_FOUND AND NOT ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "k1om")
    target_link_libraries(baar_server ${CURSES_LIBRARIES})
endif()
