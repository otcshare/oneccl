#
# Copyright 2016-2020 Intel Corporation
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
cmake_minimum_required (VERSION 2.8)

set(PROJECT_NAME "oneCCL regression tests")
project(${PROJECT_NAME})

message(STATUS "FT PROJECT_NAME: ${PROJECT_NAME}")
message(STATUS "FT PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")

set(EXAMPLES_DIR ${PROJECT_SOURCE_DIR}/../../examples)
set(EXAMPLES_INC_DIRS ${EXAMPLES_DIR}/include)

if (NOT DEFINED ENV{EXCLUDE_LIST})
    set(EXCLUDE_LIST "${PROJECT_SOURCE_DIR}/exclude_list")
else()
    set(EXCLUDE_LIST "$ENV{EXCLUDE_LIST}")
endif()

if (DEFINED ENV{I_MPI_ROOT})
    set(I_MPI_ROOT "$ENV{I_MPI_ROOT}")
endif()

message(STATUS "FT CCL_ROOT: ${CCL_ROOT}")
message(STATUS "FT EXAMPLES_INC_DIRS: ${EXAMPLES_INC_DIRS}")
message(STATUS "EXCLUDE_LIST: ${EXCLUDE_LIST}")

FILE(READ "${EXCLUDE_LIST}" EXCLUDE_TESTS)
STRING(REGEX REPLACE "\n" ";" EXCLUDE_TESTS "${EXCLUDE_TESTS}")
message(STATUS "EXCLUDE_TESTS: ${EXCLUDE_TESTS}")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (${CMAKE_C_COMPILER_ID} STREQUAL "Clang" AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR(${CMAKE_CXX_COMPILER_ID} STREQUAL "IntelLLVM"))
    set(CMAKE_CXX_STANDARD 17)
endif()

add_subdirectory(common)
if (${CMAKE_C_COMPILER_ID} STREQUAL "Clang" AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR(${CMAKE_CXX_COMPILER_ID} STREQUAL "IntelLLVM"))
    add_subdirectory(sycl)
endif()

