cmake_minimum_required (VERSION 3.10)

INCLUDE( CheckIncludeFiles )
INCLUDE( CheckLibraryExists )
INCLUDE( CheckFunctionExists )
INCLUDE( CheckSymbolExists )
INCLUDE( CheckCXXCompilerFlag )

include(CMakeForceCompiler)
set( CMAKE_C_COMPILER /opt/rocm/llvm/bin/clang)
set( CMAKE_CXX_COMPILER /opt/rocm/llvm/bin/clang++)
set( CMAKE_CXX_FLAGS "-O0 -ggdb")

set( CMAKE_INCLUDE_CURRENT_DIR ON )

set(PROJ_NAME "kwssh")
set(PROJ_H_FILES kwssh.h KwMacro.h)
#set(PROJ_H_FILES ${PROJ_H_FILES} other.h)
set(PROJ_C_FILES kwssh.c kwsshtest.c)
#set(PROJ_C_FILES ${PROJ_C_FILES} other.c)

# KL:20241005 include db/odbc/kwuty to resolve kwjson.h:21:10: fatal error: 'KwMacro.h' file not found
set(PROJ_INCLUDE_DIRS "$ENV{HOME}/misc/workspace/db/odbc/kwuty" "$ENV{HOME}/misc/workspace/db/odbc")

project( ${PROJ_NAME} )
include_directories( ${PROJ_INCLUDE_DIRS} )
set(SOURCE_FILES ${PROJ_H_FILES} ${PROJ_C_FILES})

#
#	Build static link library file
#
add_library(${PROJ_NAME}_STATIC STATIC ${PROJ_C_FILES})
set_target_properties(${PROJ_NAME}_STATIC PROPERTIES OUTPUT_NAME ${PROJ_NAME})
target_include_directories(${PROJ_NAME}_STATIC PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "${PROJ_INCLUDE_DIRS}")

#
#	Build dynamic link library file
#
add_library(${PROJ_NAME}_SHARED SHARED ${PROJ_C_FILES})
set_target_properties(${PROJ_NAME}_SHARED PROPERTIES OUTPUT_NAME ${PROJ_NAME})
target_include_directories(${PROJ_NAME}_SHARED PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} "${PROJ_INCLUDE_DIRS}")

#
#	Add Testing Build
#		Change to ${PROJ_NAME}_STATIC to build using static library
#
#	Ref: https://cmake.org/cmake/help/latest/command/target_link_libraries.html
#
add_executable( kwsshtest kwsshtest.c )
target_link_libraries(kwsshtest ${PROJ_NAME}_SHARED )
target_link_libraries(kwsshtest libssh.so libssl.so )
target_include_directories(kwsshtest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} "${PROJ_INCLUDE_DIRS}")
add_dependencies( kwsshtest ${PROJ_NAME}_SHARED )
set_property(TARGET kwsshtest PROPERTY CXX_STANDARD 20 )



