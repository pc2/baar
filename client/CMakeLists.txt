add_subdirectory(pass)

add_executable(baar_client abstractclient.cpp shmemclient.cpp socketclient.cpp main.cpp)

target_link_libraries(baar_client baar_common baar_client_passes mpi)

find_library(LLVMX86_FOUND LLVMX86Utils PATHS ${LLVM_BIN_DIR}/lib)
if(LLVMX86_FOUND)
    target_link_libraries(baar_client
        LLVMX86Disassembler
        LLVMX86AsmParser
        LLVMX86CodeGen
        LLVMX86Desc
        LLVMX86Info
        LLVMX86AsmPrinter
        LLVMX86Utils
    )
endif()

target_link_libraries(baar_client
        LLVMPolly
        LLVMIRReader
        LLVMBitReader
        LLVMBitWriter
        LLVMAsmParser
        LLVMSelectionDAG
        LLVMAsmPrinter
        LLVMMCParser
        LLVMJIT
        LLVMRuntimeDyld
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

target_link_libraries(baar_client ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})

if(UNIX AND NOT APPLE)
    target_link_libraries(baar_client rt)
endif()

#if(CURSES_FOUND AND NOT ${CMAKE_SYSTEM_PROCESSOR} STREQUAL "k1om")
#    target_link_libraries(baar_client ${CURSES_LIBRARIES})
#endif()
