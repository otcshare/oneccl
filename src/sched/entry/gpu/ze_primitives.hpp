/*
 Copyright 2016-2020 Intel Corporation
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
     http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#pragma once

#include "common/log/log.hpp"
#include "ze_call.hpp"

#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>
#include <ze_api.h>

namespace ccl {

namespace ze {

#define ZE_CALL(ze_name, ze_args) \
    do { \
        ccl::ze::ze_call ze; \
        ze.do_call(ze_name ze_args, #ze_name); \
    } while (0);

constexpr ze_fence_desc_t default_fence_desc = { .stype = ZE_STRUCTURE_TYPE_FENCE_DESC,
                                                 .pNext = NULL,
                                                 .flags = 0 };

constexpr ze_kernel_desc_t default_kernel_desc = { .stype = ZE_STRUCTURE_TYPE_KERNEL_DESC,
                                                   .pNext = nullptr,
                                                   .flags = 0,
                                                   .pKernelName = nullptr };

constexpr ze_command_list_desc_t default_cmd_list_desc = {
    .stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC,
    .pNext = NULL,
    .commandQueueGroupOrdinal = 0,
    .flags = 0,
};

constexpr ze_command_queue_desc_t default_comp_queue_desc = {
    .stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
    .pNext = nullptr,
    .ordinal = 0,
    .index = 0,
    .flags = 0,
    .mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
    .priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL
};

constexpr ze_device_mem_alloc_desc_t default_device_mem_alloc_desc = {
    .stype = ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC,
    .pNext = NULL,
    .flags = 0,
    .ordinal = 0
};

void load_module(std::string dir,
                 std::string file_name,
                 ze_device_handle_t device,
                 ze_context_handle_t context,
                 ze_module_handle_t* module);
void create_kernel(ze_module_handle_t module, std::string kernel_name, ze_kernel_handle_t* kernel);

// This structure is just to align with l0 ze_group_count_t for convenience. l0 has no ze_group_size_t
struct ze_group_size_t {
    uint32_t groupSizeX = 0;
    uint32_t groupSizeY = 0;
    uint32_t groupSizeZ = 0;
};

void get_suggested_group_size(ze_kernel_handle_t kernel, size_t count, ze_group_size_t* group_size);
void get_suggested_group_count(const ze_group_size_t& group_size,
                               size_t count,
                               ze_group_count_t* group_count);

using ze_kernel_arg_t = std::pair<size_t, const void*>;
using ze_kernel_args_t = std::initializer_list<ze_kernel_arg_t>;
void set_kernel_args(ze_kernel_handle_t kernel, const ze_kernel_args_t& kernel_args);

using ze_queue_properties_t = std::vector<ze_command_queue_group_properties_t>;

void get_num_queue_groups(ze_device_handle_t device, uint32_t* num);
void get_queues_properties(ze_device_handle_t device,
                           uint32_t num_queue_groups,
                           ze_queue_properties_t* props);
void get_comp_queue_ordinal(ze_device_handle_t device,
                            const ze_queue_properties_t& props,
                            uint32_t* ordinal);
void get_queue_index(const ze_queue_properties_t& props,
                     uint32_t ordinal,
                     int rank,
                     uint32_t* index);

std::string to_string(const ze_result_t result);
std::string to_string(const ze_group_size_t& group_size);
std::string to_string(const ze_group_count_t& group_count);
std::string to_string(const ze_kernel_args_t& kernel_args);
} // namespace ze
} // namespace ccl