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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(dcm
    src/main.c
)
