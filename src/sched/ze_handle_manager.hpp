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

#include "oneapi/ccl/config.h"

#if defined(CCL_ENABLE_SYCL) && defined(MULTI_GPU_SUPPORT)
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#include <utility>

#include "common/utils/buffer.hpp"
#include "common/log/log.hpp"
#include "common/stream/stream.hpp"

#include <ze_api.h>
#include <CL/sycl/backend/level_zero.hpp>

struct ipc_handle_info {
    ze_ipc_mem_handle_t handle;
    size_t mem_offset;
};

class ze_handle_manager {
public:
    ze_handle_manager() {}

    ~ze_handle_manager() {
        // TODO: fix cleanup. it throw when close fd
        // clear();
    }

    void init(const ccl_stream* stream);
    void clear();

    void set(const std::vector<std::vector<ipc_handle_info>>& handles_arg);
    void get(const int rank, const size_t buf_idx, ccl_buffer& buf);

private:
    ze_context_handle_t context;
    ze_device_handle_t device;
    std::vector<std::vector<ipc_handle_info>> handles;
    std::vector<std::pair<void*, ipc_handle_info>> opened_handles;
};
#endif // CCL_ENABLE_SYCL && MULTI_GPU_SUPPORT
