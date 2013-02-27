cmake_minimum_required(VERSION 2.6)

project(dcm)

if(UNIX)
    add_definitions(-g)
endif()

if(WIN32)
    add_definitions(/wd4996)
endif()

set(DYN_LIBS_ONLY true)
add_subdirectory(ext/dyn)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/dyn/src)

set(PCRE_MINIMAL_DEFAULT "OFF")
add_subdirectory(ext/pcre)
add_subdirectory(ext/genHeader)
add_subdirectory(ext/lua)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/lua/src)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

set(SOURCES
    src/dcmVariant.c
    src/main.c
)
genHeader(SOURCES src/dcmBase.lua ${CMAKE_CURRENT_BINARY_DIR}/dcmBaseLua.h dcmBaseLua)
add_executable(dcm ${SOURCES})
target_link_libraries(dcm dyn lua pcre)

if(UNIX)
    target_link_libraries(dcm m)
endif()
