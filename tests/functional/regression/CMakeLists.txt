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
set(sources
    allgatherv_vector.cpp
    allreduce_events.cpp
    alltoallv_empty_count.cpp)

set(CCL_INSTALL_TESTS "${CMAKE_CURRENT_BINARY_DIR}")

foreach(src ${sources})
    get_filename_component(executable ${src} NAME_WE)
    add_executable(${executable} ${src})
    target_link_libraries(${executable} PRIVATE ccl gtest_main gtest mpi)
    install(TARGETS ${executable} RUNTIME DESTINATION ${CCL_INSTALL_TESTS} OPTIONAL)
endforeach(src ${sources})

add_test(NAME alltoallv_empty_count CONFIGURATIONS regression COMMAND mpiexec.hydra -l -n 3 -ppn 1 ${CCL_INSTALL_TESTS}/alltoallv_empty_count --gtest_output=xml:${CCL_INSTALL_TESTS}/alltoallv_empty_count_default_report.junit.xml)

add_test(NAME allgatherv_vector CONFIGURATIONS regression COMMAND mpiexec.hydra -l -n 4 -ppn 1 ${CCL_INSTALL_TESTS}/allgatherv_vector --gtest_output=xml:${CCL_INSTALL_TESTS}/allgatherv_vector_default_report.junit.xml)
add_test(NAME allreduce_events CONFIGURATIONS regression COMMAND mpiexec.hydra -l -n 2 -ppn 1 ${CCL_INSTALL_TESTS}/allreduce_events --gtest_output=xml:${CCL_INSTALL_TESTS}/allgatherv_vector_default_report.junit.xml)
