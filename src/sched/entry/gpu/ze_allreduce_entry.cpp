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
#include "common/comm/l0/modules/kernel_utils.hpp"
#include "common/stream/stream.hpp"
#include "sched/queue/queue.hpp"
#include "ze_primitives.hpp"
#include "ze_cache.hpp"
#include "ze_allreduce_entry.hpp"

#include <CL/sycl/backend/level_zero.hpp>
#include <string>

using namespace ccl;
using namespace ccl::ze;

ze_allreduce_entry::ze_allreduce_entry(ccl_sched* sched,
                                       ccl_buffer send_buf,
                                       ccl_buffer recv_buf,
                                       size_t cnt,
                                       const ccl_datatype& dtype,
                                       reduction op,
                                       ccl_comm* comm)
        : sched_entry(sched),
          sched(sched),
          send_buf(send_buf),
          recv_buf(recv_buf),
          rank(comm->rank()),
          comm_size(comm->size()),
          cnt(cnt),
          dtype(dtype),
          op(op),
          buf_size_bytes(dtype.size() * cnt),
          is_initialized(false),
          worker_idx(0) {}

ze_allreduce_entry::~ze_allreduce_entry() {
    finalize();
}

void ze_allreduce_entry::init() {
    if (is_initialized) {
        return;
    }

    LOG_DEBUG("initialization");

    CCL_THROW_IF_NOT(sched != nullptr, "no sched");
    worker_idx = sched->queue->get_idx();

    CCL_THROW_IF_NOT(sched->coll_param.stream, "null stream in ze_allreduce_entry");

    LOG_DEBUG("getting a native stream");
    auto native_stream = sched->coll_param.stream->get_native_stream(worker_idx);
    if (native_stream->get_backend() != sycl::backend::level_zero) {
        CCL_THROW("unsupported sycl backend");
    }

    auto sycl_device = native_stream->get_device();
    device = sycl_device.template get_native<sycl::backend::level_zero>();

    auto sycl_context = native_stream->get_context();
    context = sycl_context.template get_native<sycl::backend::level_zero>();

    uint32_t num_queue_groups;
    get_num_queue_groups(device, &num_queue_groups);

    ze_queue_properties_t queue_props;
    get_queues_properties(device, num_queue_groups, &queue_props);

    uint32_t comp_ordinal, comp_queue_index;
    get_comp_queue_ordinal(device, queue_props, &comp_ordinal);
    get_queue_index(queue_props, comp_ordinal, rank, &comp_queue_index);

    comp_queue_desc = default_comp_queue_desc;
    comp_queue_desc.index = comp_queue_index;
    comp_queue_desc.ordinal = comp_ordinal;

    ccl::global_data::get().ze_cache->get(
        worker_idx, context, device, &comp_queue_desc, &comp_queue);
    LOG_DEBUG("get compute queue: { ordinal: ",
              comp_queue_desc.ordinal,
              ", index: ",
              comp_queue_desc.index,
              " }");

    comp_list_desc = default_cmd_list_desc;
    comp_list_desc.commandQueueGroupOrdinal = comp_ordinal;
    ccl::global_data::get().ze_cache->get(worker_idx, context, device, &comp_list_desc, &comp_list);
    LOG_DEBUG("get compute list: { ordinal: ", comp_list_desc.commandQueueGroupOrdinal, " }");

    ccl::global_data::get().ze_cache->get(context, device, &module);

    kernel_name = "allreduce_execution_" + to_string(dtype.idx()) + "_" + ccl_reduction_to_str(op);
    ccl::global_data::get().ze_cache->get(worker_idx, module, kernel_name, &kernel);
    LOG_DEBUG("get kernel: name: ", kernel_name);

    ze_group_size_t group_size;
    get_suggested_group_size(kernel, cnt, &group_size);
    LOG_DEBUG("suggested group size: ", to_string(group_size));

    get_suggested_group_count(group_size, cnt, &group_count);
    LOG_DEBUG("suggested group count: ", to_string(group_count));

    ZE_CALL(zeKernelSetGroupSize,
            (kernel, group_size.groupSizeX, group_size.groupSizeY, group_size.groupSizeZ));

    ccl_buffer right_send_buf;
    ccl_buffer right_recv_buf;
    int peer_rank = (rank + 1) % comm_size;
    sched->get_memory().handle_manager.get(peer_rank, 0, right_send_buf);
    sched->get_memory().handle_manager.get(peer_rank, 1, right_recv_buf);
    LOG_DEBUG("get IPC pointers from ",
              peer_rank,
              " rank: right_send_buf: ",
              right_send_buf,
              ", right_recv_buf: ",
              right_recv_buf);

    send_buf_ptr = send_buf.get_ptr();
    recv_buf_ptr = recv_buf.get_ptr();
    right_send_buf_ptr = right_send_buf.get_ptr();
    right_recv_buf_ptr = right_recv_buf.get_ptr();
    ze_kernel_args_t kernel_args = { { sizeof(rank), &rank },
                                     { sizeof(comm_size), &comm_size },
                                     { sizeof(cnt), &cnt },
                                     { sizeof(send_buf_ptr), &send_buf_ptr },
                                     { sizeof(recv_buf_ptr), &recv_buf_ptr },
                                     { sizeof(right_send_buf_ptr), &right_send_buf_ptr },
                                     { sizeof(right_recv_buf_ptr), &right_recv_buf_ptr } };

    LOG_DEBUG("kernel ", kernel, " args:\n", to_string(kernel_args));
    set_kernel_args(kernel, kernel_args);

    ZE_CALL(zeCommandListAppendLaunchKernel,
            (comp_list, kernel, &group_count, nullptr, 0, nullptr));
    ZE_CALL(zeCommandListClose, (comp_list));

    fence_desc = default_fence_desc;
    ccl::global_data::get().ze_cache->get(worker_idx, comp_queue, &fence_desc, &fence);

    is_initialized = true;

    LOG_DEBUG("initialization complete");
}

void ze_allreduce_entry::start() {
    init();

    if (is_initialized && status == ccl_sched_entry_status_not_started) {
        ZE_CALL(zeFenceReset, (fence));
    }

    size_t kernel_counter = 0;
    if (ccl::global_data::env().enable_kernel_sync) {
        kernel_counter = std::atomic_fetch_add<size_t>(&ccl::global_data::get().kernel_counter, 1);
    }

    if (kernel_counter == 0) {
        LOG_DEBUG("execute command list");
        ZE_CALL(zeCommandQueueExecuteCommandLists, (comp_queue, 1, &comp_list, fence));

        if (((global_data::env().ze_serialize_mode & ze_serialize_block)) != 0) {
            LOG_DEBUG("wait until command lists are executed");
            ZE_CALL(zeHostSynchronize, (comp_queue));
        }
        status = ccl_sched_entry_status_started;
    }
    else {
        std::atomic_fetch_sub<size_t>(&ccl::global_data::get().kernel_counter, 1);
        status = ccl_sched_entry_status_again;
    }
}

void ze_allreduce_entry::update() {
    ze_result_t fence_status;
    if (global_data::env().kernel_debug == 0) {
        fence_status = zeFenceQueryStatus(fence);
    }
    else {
        fence_status = zeCommandQueueSynchronize(comp_queue, std::numeric_limits<uint64_t>::max());
    }

    if (fence_status == ZE_RESULT_SUCCESS) {
        LOG_DEBUG("command list complete");
        if (!sched->coll_attr.to_cache) {
            finalize();
        }
        status = ccl_sched_entry_status_complete;
    }
    else if (fence_status == ZE_RESULT_NOT_READY) {
        // just return in case if the kernel is not ready yet, will check again on the next iteration
        return;
    }
    else {
        CCL_THROW("error at zeFenceQueryStatus");
    }

    if (ccl::global_data::get().kernel_counter > 0) {
        std::atomic_fetch_sub<size_t>(&ccl::global_data::get().kernel_counter, 1);
    }
}

void ze_allreduce_entry::finalize() {
    if (!is_initialized) {
        return;
    }

    LOG_DEBUG("finalization");

    ccl::global_data::get().ze_cache->push(worker_idx, comp_queue, &fence_desc, &fence);
    ccl::global_data::get().ze_cache->push(worker_idx, module, kernel_name, &kernel);
    ccl::global_data::get().ze_cache->push(
        worker_idx, context, device, &comp_list_desc, &comp_list);
    ccl::global_data::get().ze_cache->push(
        worker_idx, context, device, &comp_queue_desc, &comp_queue);

    is_initialized = false;

    LOG_DEBUG("finalization complete");
}