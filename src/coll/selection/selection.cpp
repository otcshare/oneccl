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
#include "coll/selection/selection.hpp"
#include "common/global/global.hpp"

bool ccl_is_direct_algo(const ccl_selector_param& param) {
    bool res = false;

    auto& selector = ccl::global_data::get().algorithm_selector;

    if (param.ctype == ccl_coll_allgatherv) {
        res = (selector->get<ccl_coll_allgatherv>(param) == ccl_coll_allgatherv_direct);
    }
    else if (param.ctype == ccl_coll_allreduce) {
        res = (selector->get<ccl_coll_allreduce>(param) == ccl_coll_allreduce_direct);
    }
    else if (param.ctype == ccl_coll_alltoall) {
        res = (selector->get<ccl_coll_alltoall>(param) == ccl_coll_alltoall_direct);
    }
    else if (param.ctype == ccl_coll_alltoallv) {
        res = (selector->get<ccl_coll_alltoallv>(param) == ccl_coll_alltoallv_direct);
    }
    else if (param.ctype == ccl_coll_barrier) {
        res = (selector->get<ccl_coll_barrier>(param) == ccl_coll_barrier_direct);
    }
    else if (param.ctype == ccl_coll_bcast) {
        res = (selector->get<ccl_coll_bcast>(param) == ccl_coll_bcast_direct);
    }
    else if (param.ctype == ccl_coll_reduce) {
        res = (selector->get<ccl_coll_reduce>(param) == ccl_coll_reduce_direct);
    }
    else if (param.ctype == ccl_coll_reduce_scatter) {
        res = (selector->get<ccl_coll_reduce_scatter>(param) == ccl_coll_reduce_scatter_direct);
    }

    return res;
}

bool ccl_is_topo_ring_algo(const ccl_selector_param& param) {
#ifndef CCL_ENABLE_SYCL
    return false;
#endif // CCL_ENABLE_SYCL

    if ((param.ctype != ccl_coll_allreduce) && (param.ctype != ccl_coll_bcast) &&
        (param.ctype != ccl_coll_reduce)) {
        return false;
    }

    bool res = false;

    auto& selector = ccl::global_data::get().algorithm_selector;

    if (param.ctype == ccl_coll_allreduce) {
        res = (selector->get<ccl_coll_allreduce>(param) == ccl_coll_allreduce_topo_ring);
    }
    else if (param.ctype == ccl_coll_bcast) {
        res = (selector->get<ccl_coll_bcast>(param) == ccl_coll_bcast_topo_ring);
    }
    else if (param.ctype == ccl_coll_reduce) {
        res = (selector->get<ccl_coll_reduce>(param) == ccl_coll_reduce_topo_ring);
    }

    return res;
}

bool ccl_can_use_topo_ring_algo(const ccl_selector_param& param) {
    if ((param.ctype != ccl_coll_allreduce) && (param.ctype != ccl_coll_bcast) &&
        (param.ctype != ccl_coll_reduce)) {
        return false;
    }

    bool is_sycl_buf = false;
    bool is_device_buf = true;
    bool is_l0_backend = false;

    size_t local_proc_count = ccl::global_data::get().executor->get_local_proc_count();

#ifdef CCL_ENABLE_SYCL
    is_sycl_buf = param.is_sycl_buf;
    if (param.buf && param.stream) {
        auto ctx = param.stream->get_native_stream().get_context();
        is_device_buf =
            (sycl::get_pointer_type(param.buf, ctx) == sycl::usm::alloc::device) ? true : false;
    }
#ifdef MULTI_GPU_SUPPORT
    if (param.stream && param.stream->get_backend() == sycl::backend::level_zero) {
        is_l0_backend = true;
    }
#endif // MULTI_GPU_SUPPORT
#endif // CCL_ENABLE_SYCL

    if ((param.comm->size() != 2 && param.comm->size() != 4) ||
        (param.comm->size() == 2 && param.comm->size() != static_cast<int>(local_proc_count)) ||
        (param.comm->size() == 4 && local_proc_count != 2 && local_proc_count != 4) ||
        (param.comm->size() != 2 && (ccl::global_data::env().atl_transport == ccl_atl_mpi)) ||
        !param.stream || (param.stream->get_type() != stream_type::gpu) || is_sycl_buf ||
        !is_device_buf || !is_l0_backend || ccl::global_data::env().enable_fusion ||
        ccl::global_data::env().enable_unordered_coll ||
        (ccl::global_data::env().priority_mode != ccl_priority_none) ||
        (ccl::global_data::env().worker_count != 1)) {
        return false;
    }

    return true;
}