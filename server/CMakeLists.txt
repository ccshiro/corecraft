#
# Copyright (C) 2015 shiro <shiro@worldofcorecraft.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

project(MaNGOS)
cmake_minimum_required(VERSION 2.8.12)

#################
# CMake options #
#################

option(ENABLE_OPTIMIZED "Enable optimizations" OFF)
option(ENABLE_ASSERTS "Enable assert()" ON)
option(ENABLE_SAMPLING "Enable various performance sampling" OFF)
option(TESTS "Build unittests" ON)
option(TOOLS "Build tools to extract game-data" OFF)
# CXX_COMPILER_FLAGS: Extra command-line options passed onto C++ compiler
# LINKER_FLAGS: Extra command-line options passed onto linker

########################
# CMake configurations #
########################

set(CMAKE_MODULE_PATH
    ${CMAKE_MODULE_PATH}
    ${CMAKE_SOURCE_DIR}/cmake/modules
)

# Force out-of-source build
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" insource)
if (insource)
    message(FATAL_ERROR "This project requires out-of-source building.")
endif ()

# Install path
set(CMAKE_INSTALL_PREFIX "/usr/local" CACHE STRING "prefix")
set(BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
set(LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
set(CONF_DIR ${CMAKE_INSTALL_PREFIX}/etc)
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

#########################
# Third Party Libraries #
#########################

# Boost
set(Boost_USE_MULTITHREADED ON)
if (WIN32)
	set(Boost_USE_STATIC_LIBS ON)
    find_package(Boost COMPONENTS thread date_time system chrono regex program_options filesystem REQUIRED)
else ()
    find_package(Boost COMPONENTS thread system chrono program_options filesystem REQUIRED)
endif ()

# MySQL
find_package(MySQL REQUIRED)

# OpenSSL
find_package(OpenSSL REQUIRED)

# Zlib
find_package(ZLIB REQUIRED)

# Bzip2
find_package(BZip2 REQUIRED)

# Jemalloc
find_package(Jemalloc REQUIRED)

# Google's sparsehash
find_path(SPARSEHASH_INCLUDE_DIRS sparsehash/dense_hash_map)
if (NOT SPARSEHASH_INCLUDE_DIRS)
    message(FATAL_ERROR "Could not find <sparsehash/dense_hash_map>.")
endif ()

######################
# Build Instructions #
######################

# Compiler-specific instructions
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang instructions
    if (ENABLE_OPTIMIZED)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    endif ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wfatal-errors -Wextra -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -std=c++14")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC Instructions
    if (ENABLE_OPTIMIZED)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    endif ()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wfatal-errors -Wextra -g")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-format-zero-length")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -std=c++14")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # MSVC instructions
    # TODO
else ()
    # Unrecognized compiler
    message(FATAL_ERROR "Unsupported compiler.")
endif ()

# Link Time Optimization
if (ENABLE_OPTIMIZED)
    # GCC LTO
    if (CMAKE_COMPILER_IS_GNUCXX)
        find_program(GCC_AR_BIN "gcc-ar")
        find_program(GCC_RANLIB_BIN "gcc-ranlib")
        if (GCC_AR_BIN MATCHES "GCC_AR_BIN-NOTOFUND" OR
            GCC_RANLIB_BIN MATCHES "GCC_RANLIB_BIN-NOTFOUND")
            message(FATAL_ERROR "Unable to find gcc-ar and gcc-ranlib")
        endif ()

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -O3")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto -O3")
        # TODO: Specifying these linker options is useless for us right?
        #set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto -O3")
        #set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -flto -O3")
        #set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -flto -O3")
        set(CMAKE_AR ${GCC_AR_BIN})
        set(CMAKE_RANLIB ${GCC_RANLIB_BIN})
    # Clang LTO (not tested yet!)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        find_program(CLANG_AR_BIN "llvm-ar")
        find_program(CLANG_RANLIB_BIN "llvm-ranlib")
        if (CLANG_AR_BIN MATCHES "CLANG_AR_BIN-NOTOFUND" OR
                CLANG_RANLIB_BIN MATCHES "CLANG_RANLIB_BIN-NOTFOUND")
            message(FATAL_ERROR "Unable to find llvm-ar and llvm-ranlib")
        endif ()

        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -O3")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto -O3")
        # TODO: Specifying these linker options is useless for us right?
        #set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto -O3")
        #set(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS} -flto -O3")
        #set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -flto -O3")
        set(CMAKE_AR ${CLANG_AR_BIN})
        set(CMAKE_RANLIB ${CLANG_RANLIB_BIN})
    else ()
        message(FATAL_ERROR "Unable to turn on Link Time Optimization")
    endif ()
endif ()

# User-defined compiler and linker settings
if (CXX_COMPILER_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX_COMPILER_FLAGS}")
endif ()
if (LINKER_FLAGS)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LINKER_FLAGS}")
endif ()

# Build dep and utils before we star adding our own include and defines
add_subdirectory(dep)
add_subdirectory(utils)

# Include directories
include_directories(SYSTEM ${SPARSEHASH_INCLUDE_DIRS})
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})
include_directories(SYSTEM ${ZLIB_INCLUDE_DIRS})
# FIXME: Remove this and put include files in include/
include_directories(src)

# Rpath settings
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Preprocessor defines
if (ENABLE_OPTIMIZED)
    add_definitions(-DOPTIMIZED_BUILD)
endif ()
if (NOT ENABLE_ASSERTS)
    add_definitions(-DNDEBUG)
endif()
if (ENABLE_SAMPLING)
    add_definitions(-DPERF_SAMPLING_MAP_UPDATE)
endif ()
add_definitions(-DSYSCONFDIR=\"etc/\")

# Build our sub-directories
add_subdirectory(src)
add_subdirectory(contrib)
if (TESTS)
    add_subdirectory(unittests)
endif()
