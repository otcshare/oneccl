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

#include "common/comm/comm.hpp"
#include "common/global/global.hpp"
#include "common/utils/buffer.hpp"
#include "comp/comp.hpp"
#include "sched/entry/entry.hpp"

#include <sstream>
#include <ze_api.h>

class ze_allreduce_entry : public sched_entry {
public:
    static constexpr const char* class_name() noexcept {
        return "ZE_ALLREDUCE";
    }

    const char* name() const noexcept override {
        return class_name();
    }

    bool is_gpu_entry() const noexcept override {
        return true;
    }

    ze_allreduce_entry() = delete;
    explicit ze_allreduce_entry(ccl_sched* sched,
                                ccl_buffer send_buf,
                                ccl_buffer recv_buf,
                                size_t cnt,
                                const ccl_datatype& dtype,
                                ccl::reduction op,
                                ccl_comm* comm);

    void init();
    void start() override;
    void update() override;
    void finalize();

protected:
    void dump_detail(std::stringstream& str) const override {
        ccl_logger::format(str,
                           "dt ",
                           ccl::global_data::get().dtypes->name(dtype),
                           ", cnt ",
                           cnt,
                           ", send_buf ",
                           send_buf,
                           ", recv_buf ",
                           recv_buf,
                           ", op ",
                           ccl_reduction_to_str(op),
                           ", comm_id ",
                           sched->get_comm_id(),
                           ", context ",
                           context,
                           "\n");
    }

private:
    ccl_sched* sched;
    ccl_buffer send_buf;
    ccl_buffer recv_buf;
    void* send_buf_ptr;
    void* recv_buf_ptr;
    void* right_send_buf_ptr;
    void* right_recv_buf_ptr;
    const int rank;
    const int comm_size;
    const unsigned long cnt;
    const ccl_datatype dtype;
    const ccl::reduction op;
    const size_t buf_size_bytes;
    ze_context_handle_t context;
    ze_device_handle_t device;
    ze_command_queue_handle_t comp_queue;
    ze_command_list_handle_t comp_list;
    ze_module_handle_t module;
    ze_kernel_handle_t kernel;
    ze_group_count_t group_count;
    ze_fence_handle_t fence;
};
