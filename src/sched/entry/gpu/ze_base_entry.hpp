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
#include "sched/sched.hpp"
#include "sched/entry/entry.hpp"

#include <ze_api.h>

class ze_base_entry : public sched_entry {
public:
    bool is_gpu_entry() const noexcept override {
        return true;
    }

    ze_base_entry() = delete;
    ze_base_entry(const ze_base_entry&) = delete;
    virtual ~ze_base_entry(){};

protected:
    explicit ze_base_entry(ccl_sched* sched, uint32_t add_event_count = 0);

    virtual void init();
    virtual void start() override;
    virtual void update() override;
    virtual void finalize();

    ccl_sched* const sched;
    const uint32_t add_event_count;
    int rank{};
    int comm_size{};
    size_t worker_idx{};

    ze_device_handle_t device{};
    ze_context_handle_t context{};

    ze_command_queue_desc_t comp_queue_desc{};
    ze_command_queue_handle_t comp_queue{};

    ze_command_list_desc_t comp_list_desc{};
    ze_command_list_handle_t comp_list{};

    ze_event_pool_desc_t event_pool_desc{};
    ze_event_pool_handle_t event_pool{};

    ze_event_handle_t entry_event{};

private:
    bool is_initialized{};
};
