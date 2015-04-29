add_definitions(-DLLVM_3_4) # TODO

add_library(cbackend cbackend.cpp ispcadditions.cpp)

if(${CMAKE_SYSTEM_PROCESSOR} STREQUAL "k1om")
    add_custom_command(TARGET cbackend PRE_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_CURRENT_SOURCE_DIR}/knc-i1x16.h ${CMAKE_BINARY_DIR}/out/bin)
else()
    add_custom_command(TARGET cbackend PRE_BUILD
                       COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_CURRENT_SOURCE_DIR}/generic-16.h ${CMAKE_BINARY_DIR}/out/bin)
endif()
