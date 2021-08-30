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
cmake_minimum_required(VERSION 2.8)

set(pkg_sources
    pkgconfig_support_checker.cpp)

set(CCL_INSTALL_TESTS "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(PkgConfig REQUIRED)
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "IntelLLVM")
    set(PKG_CCL_LIB "ccl-cpu_gpu_dpcpp")
else()
    set(PKG_CCL_LIB "ccl-cpu_icc")
endif()
pkg_check_modules(IMPI_PKG REQUIRED IMPORTED_TARGET impi)
pkg_check_modules(CCL_PKG REQUIRED IMPORTED_TARGET ${PKG_CCL_LIB})

message(STATUS "CCL_PKG_LDFLAGS = ${CCL_PKG_LDFLAGS}")
message(STATUS "CCL_PKG_CFLAGS = ${CCL_PKG_CFLAGS}")

foreach(src ${pkg_sources})
   get_filename_component(executable ${src} NAME_WE)
   add_executable(${executable} ${src})
   target_link_libraries(${executable} PRIVATE ${CCL_PKG_LDFLAGS})
   target_include_directories(${executable} PRIVATE ${CCL_PKG_CFLAGS})
   install(TARGETS ${executable} RUNTIME DESTINATION ${CCL_INSTALL_TESTS} OPTIONAL)
endforeach(src ${pkg_sources})
