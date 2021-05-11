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

#include <initializer_list>
#include <atomic>

#include "sched/entry/l0/l0_entry.hpp"
#include "common/comm/l0/context/scale/ipc/ipc_ctx_impl.hpp"
#include "kernels/shared.h"

namespace native {
template <class gpu_comm_impl, ccl::group_split_type topology>
class l0_allreduce_typed_entry : public base_gpu_entry<gpu_comm_impl,
                                                       topology,
                                                       ccl::device_topology_type::ring,
                                                       ccl_coll_allreduce> {
public:
    friend class ccl_gpu_comm;
    friend class ccl_virtual_gpu_comm;

    using base = base_gpu_entry<gpu_comm_impl,
                                topology,
                                ccl::device_topology_type::ring,
                                ccl_coll_allreduce>;
    using base::parent_communicator;
    using base::comm_addr;
    using base::req;
    using base::status;
    using base::launch_args;
    using base::kernel_router;
    using base::get_ctx;
    using base::alloc_memory_wrap;
    using base::get_local_kernel;
    using kernel_main_typed = ring::allreduce::main_kernel;

    using income_data_flag_gpu_type =
        typename std::remove_pointer<typename ring::allreduce::income_data_flag_arg_type>::type;
    using ready_to_recv_flag_gpu_type =
        typename std::remove_pointer<typename ring::allreduce::ready_to_recv_flag_arg_type>::type;
    using local_barrier_flag_gpu_type =
        typename std::remove_pointer<typename ring::allreduce::local_barrier_flag_arg_type>::type;

    static constexpr const char* class_name() noexcept {
        return "L0_ALLREDUCE_TYPED";
    }

    static constexpr ccl_coll_type type() noexcept {
        return ccl_coll_allreduce;
    }

    l0_allreduce_typed_entry() = delete;
    l0_allreduce_typed_entry(
        ccl_sched* sched,
        std::shared_ptr<gpu_comm_impl> comm,
        specific_indexed_device_storage& available_devices,
        ccl_driver_context_ptr in_ctx,
        const ccl_buffer send_buf,
        ccl_buffer recv_buf,
        size_t cnt,
        const coll_param_gpu& params,
        std::shared_ptr<ccl_stream> device_stream = std::shared_ptr<ccl_stream>())
            : base(sched, comm, in_ctx, send_buf, params, device_stream),

              temp_buffer(this->template alloc_memory_wrap(
                  typename ring::allreduce::tmp_recv_buf_arg<int8_t>{},
                  parent_communicator,
                  ring_allreduce_get_tmp_buffer_size(
                      ccl::get_datatype_size(params.get_datatype()) * cnt,
                      base::comm_addr.size),
                  get_ctx())),
              income_data_flag(
                  this->template alloc_memory_wrap(typename ring::allreduce::income_data_flag_arg{},
                                                   parent_communicator,
                                                   1,
                                                   get_ctx())),
              ready_to_recv_flag(this->template alloc_memory_wrap(
                  typename ring::allreduce::ready_to_recv_flag_arg{},
                  parent_communicator,
                  1,
                  get_ctx())),
              local_barrier_flag(parent_communicator->get_device()
                                     .template alloc_memory<local_barrier_flag_gpu_type>(
                                         1,
                                         sizeof(local_barrier_flag_gpu_type),
                                         get_ctx())) {
        recv_buf_typed_entry = recv_buf;
        cnt_entry = cnt;

        int next_rank = (comm_addr.rank + 1) % comm_addr.size;
        kernel_router = base::template create_kernel_router_for_rank<
            l0_allreduce_typed_entry<gpu_comm_impl, topology>>(
            *this, next_rank, available_devices, base::get_params());

        ENTRY_LOG_DEBUG("Init phase of current entry for ext_rank:", next_rank);

        this->set_state(gpu_entry_state::created);
    }

    ~l0_allreduce_typed_entry() {
        // TODO: remove the memory once the entry is destroyed if it's not cleared automatically
        // TODO: should we destroy handles here?
    }

    void start() override {
        ENTRY_LOG_DEBUG("Start entry, cnt ", cnt_entry);

        //Create base primitives
        base::start();

        auto& main_entry_function = get_local_kernel();

        // TODO: try to remove indirect buffer
        void* recv_buf_ptr = recv_buf_typed_entry.get_ptr();

        //create implementation specified primitives
        main_entry_function
            .template set_args<typename ring::allreduce::tmp_recv_buf_arg<void>,
                               typename ring::allreduce::income_data_flag_arg,
                               typename ring::allreduce::ready_to_recv_flag_arg,
                               typename ring::allreduce::local_barrier_flag_arg,
                               typename ring::allreduce::recv_buf_arg<void>,
                               typename kernel_main_typed::common_entry_buf_size_arg>(
                temp_buffer.get(),
                income_data_flag.get(),
                ready_to_recv_flag.get(),
                local_barrier_flag.get(),
                recv_buf_ptr,
                cnt_entry);

        // Once we filled our local parameters, we go wait for another entry to set its
        // parameters so we can use them
        this->set_state(gpu_entry_state::wait_for_entry);

        //make sure, that kernel ready for launch
        this->submit_for_execution();
        status = ccl_sched_entry_status_started;
    }

    const char* name() const override {
        return class_name();
    }

    std::vector<ccl_device::device_ipc_memory_handle> get_ipc_data() override {
        ccl_device& owned_device = parent_communicator->get_device();

        std::vector<ccl_device::device_ipc_memory_handle> ret;
        ret.reserve(3);
        ret.push_back(owned_device.create_ipc_memory_handle(temp_buffer.get(), get_ctx()));
        ret.push_back(owned_device.create_ipc_memory_handle(income_data_flag.get(), get_ctx()));
        ret.push_back(owned_device.create_ipc_memory_handle(ready_to_recv_flag.get(), get_ctx()));
        return ret;
    }

    observer::invoke_params<type()> get_numa_data() override {
        observer::producer_description in_params{
            .rank = comm_addr.rank, //TODO unused
            .comm_size = comm_addr.size, //TODO unused
            .staged_buffer_elem_count = cnt_entry,
            .context = get_ctx(),
            .device = parent_communicator->get_device(),
            .immediate_list = parent_communicator->get_device().create_immediate_cmd_list(get_ctx())
        };
        // TODO: Should get_params() be a part of in_params?
        return observer::invoke_params<type()>(std::move(in_params), base::get_params());
    }

protected:
    void dump_detail(std::stringstream& str) const override {
        base::dump_detail(str);
    }

private:
    ccl_device::device_memory<int8_t> temp_buffer;
    ccl_device::device_memory<income_data_flag_gpu_type> income_data_flag;
    ccl_device::device_memory<ready_to_recv_flag_gpu_type> ready_to_recv_flag;
    ccl_device::device_memory<local_barrier_flag_gpu_type> local_barrier_flag;
    ccl_buffer recv_buf_typed_entry;
    size_t cnt_entry;

public:
    template <class left_kernel_t, class right_kernel_t>
    bool execute(left_kernel_t& left_kernel, right_kernel_t& right_kernel) {
        bool is_right_kernel_ready =
            right_kernel.template test_args<typename ring::allreduce::tmp_recv_buf_arg<void>,
                                            typename ring::allreduce::income_data_flag_arg,
                                            typename ring::allreduce::ready_to_recv_flag_arg>();

        // Once we're sure that the parameters ready read them from the right kernel
        // Note: we not only read the parameters but also reset their 'ready' flag
        // (since we're using a destructive-copying policy) meaning that they must be stored
        // in order to be read again.
        // This is a protection to a case of multiple kernel launches
        // (i.e. the collective is ran multiple times) where we might read not up-to-date
        // values from the previous run.

        if (is_right_kernel_ready) {
            auto right_tmp_recv_buf_arg =
                right_kernel.template get_arg<typename ring::allreduce::tmp_recv_buf_arg<void>>();
            auto right_income_data_flag_arg =
                right_kernel.template get_arg<typename ring::allreduce::income_data_flag_arg>();
            auto right_ready_to_recv_flag_arg =
                right_kernel.template get_arg<typename ring::allreduce::ready_to_recv_flag_arg>();

            /*ENTRY_LOG_DEBUG("Bind right arguments from ",
                            right_kernel_t::name(),
                            " kernel",
                            " to ",
                            left_kernel_t::name(),
                            " kernel. "
                            "Right arguments:\n{ ",
                            right_tmp_recv_buf_arg.first,
                            ", ",
                            right_tmp_recv_buf_arg.second,
                            "}\n",
                            "{ ",
                            right_income_data_flag_arg.first,
                            ", ",
                            right_income_data_flag_arg.second,
                            "}\n",
                            "{ ",
                            right_ready_to_recv_flag_arg.first,
                            ", ",
                            right_ready_to_recv_flag_arg.second,
                            "}\n");*/

            left_kernel.template set_args<typename ring::allreduce::right_tmp_recv_buf_arg<void>,
                                          typename ring::allreduce::right_income_data_flag_arg,
                                          typename ring::allreduce::right_ready_to_recv_flag_arg>(
                right_tmp_recv_buf_arg.second,
                right_income_data_flag_arg.second,
                right_ready_to_recv_flag_arg.second);

            ENTRY_LOG_DEBUG("Binding arguments between kernels is complete. ",
                            "Arguments of the left kernel after binding:\n",
                            left_kernel.to_string());
        }
        return is_right_kernel_ready;
    }
};
} // namespace native
