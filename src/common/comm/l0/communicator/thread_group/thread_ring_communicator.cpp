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
#include "oneapi/ccl.hpp"
#include "oneapi/ccl/type_traits.hpp"
#include "common/comm/l0/communicator/thread_group/thread_ring_communicator_impl.hpp"
#include "common/comm/l0/gpu_comm_attr.hpp"
#include "common/comm/l0/context/process_group_ctx.hpp"

using namespace ccl;

thread_device_group_ring_communicator::thread_device_group_ring_communicator(
    ccl::unified_device_type&& device,
    ccl::unified_context_type&& ctx,
    size_t thread_idx,
    size_t process_idx,
    const ccl::comm_split_attr& attr)
        : base_t(std::move(device), std::move(ctx), thread_idx, process_idx, /*comm_attr, */ attr) {
}

void thread_device_group_ring_communicator::visit(ccl::gpu_comm_attr& comm_attr) {
    auto process_ctx = comm_attr.get_process_context();
    auto thread_ctx = process_ctx->get_thread_context(process_id);
    auto device_ctx = thread_ctx->get_device_group_ctx(thread_id);
    (void)device_ctx;

    ctx = thread_ctx;

    //get rank & size
    auto topology = ctx->get_thread_topology<base_t::topology_class()>(thread_id);
    this->initialize_comm_addr(get_device_path(), topology);

    this->set_comm_group_id(comm_attr.get_unique_id());
}

/*
size_t thread_device_group_ring_communicator::group_size() const
{
    return get_device_count<l0::ccl_gpu_comm>() +
              / * get_device_count<ccl_concurrent<SOURCE!!!>>() + Will add further* /
            get_device_count<l0::ccl_virtual_gpu_comm>();

}
*/
ccl::event thread_device_group_ring_communicator::barrier(
    const ccl::stream::impl_value_t& stream,
    const ccl::barrier_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    throw ccl::exception(std::string(__PRETTY_FUNCTION__) + " - is not implemented yet");
}

/* allgatherv */
ccl::event thread_device_group_ring_communicator::allgatherv_impl(
    const void* send_buf,
    size_t send_count,
    void* recv_buf,
    const ccl::vector_class<size_t>& recv_counts,
    ccl::datatype dtype,
    const ccl::stream::impl_value_t& stream,
    const ccl::allgatherv_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    using namespace native;

    static constexpr ccl::group_split_type group_id = base_t::topology_type();
    static constexpr ccl::device_topology_type class_id = base_t::topology_class();

    if (!is_ready()) {
        throw ccl::exception(std::string(
            "Device communicator for group_id: " + ::to_string(group_id) +
            " is not ready yet. Not all сommunicators are created in group. Please create them before usage"));
    }

    int comm_rank = rank();
    size_t ring_index = 0;
    LOG_DEBUG("communicator for device idx: ",
              get_device_path(),
              ", rank idx: ",
              comm_rank,
              ", ring_index :",
              ring_index);

    //TODO make const!
    ccl_buffer send_entry_buffer(const_cast<void**>(&send_buf),
                                 send_count * ccl::get_datatype_size(dtype),
                                 0,
                                 ccl_buffer_type::INDIRECT);
    ccl_buffer recv_entry_buffer(
        &recv_buf, send_count * ccl::get_datatype_size(dtype), 0, ccl_buffer_type::INDIRECT);

    using community_t = typename device_community_container<class_id>::element_type;
    community_t community = device_community_impl.get_topology(ring_index);

    coll_param_gpu params(ccl_coll_allgatherv, dtype);

    return do_collective_op<group_id, class_id, l0_allgatherv_typed_entry>(
        communication_device,
        ctx,
        community,
        thread_id,
        this->get_native_context(),
        send_entry_buffer,
        send_count,
        recv_entry_buffer,
        recv_counts.data(),
        params,
        stream);
}
ccl::event thread_device_group_ring_communicator::allgatherv_impl(
    const void* send_buf,
    size_t send_count,
    const ccl::vector_class<void*>& recv_bufs,
    const ccl::vector_class<size_t>& recv_counts,
    ccl::datatype dtype,
    const ccl::stream::impl_value_t& stream,
    const ccl::allgatherv_attr& attr,

    const ccl::vector_class<ccl::event>& deps) {
    throw ccl::exception(std::string(__PRETTY_FUNCTION__) + " - is not implemented");
    return {};
}

/* allreduce */
ccl::event thread_device_group_ring_communicator::allreduce_impl(
    const void* send_buf,
    void* recv_buf,
    size_t count,
    ccl::datatype dtype,
    ccl::reduction reduction,
    const ccl::stream::impl_value_t& stream,
    const ccl::allreduce_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    using namespace native;

    static constexpr ccl::group_split_type group_id = base_t::topology_type();
    static constexpr ccl::device_topology_type class_id = base_t::topology_class();

    if (!is_ready()) {
        throw ccl::exception(std::string(
            "Device communicator for group_id: " + ::to_string(group_id) +
            " is not ready yet. Not all сommunicators are created in group. Please create them before usage"));
    }

    int comm_rank = rank();
    size_t ring_index = 0;
    LOG_DEBUG("communicator for device idx: ",
              get_device_path(),
              ", rank idx: ",
              comm_rank,
              ", ring_index :",
              ring_index);

    //TODO make const!
    ccl_buffer send_entry_buffer(const_cast<void**>(&send_buf),
                                 count * ccl::get_datatype_size(dtype),
                                 0,
                                 ccl_buffer_type::INDIRECT);
    ccl_buffer recv_entry_buffer(
        &recv_buf, count * ccl::get_datatype_size(dtype), 0, ccl_buffer_type::INDIRECT);

    using community_t = typename device_community_container<class_id>::element_type;
    community_t community = device_community_impl.get_topology(ring_index);

    const coll_param_gpu params(ccl_coll_allreduce, dtype, reduction);

    return do_collective_op<group_id, class_id, l0_allreduce_typed_entry>(
        communication_device,
        ctx,
        community,
        thread_id,
        this->get_native_context(),
        send_entry_buffer,
        recv_entry_buffer,
        count,
        params,
        stream);
}

/* alltoall */
ccl::event thread_device_group_ring_communicator::alltoall_impl(
    const void* send_buf,
    void* recv_buf,
    size_t count,
    ccl::datatype dtype,
    const ccl::stream::impl_value_t& stream,
    const ccl::alltoall_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    throw ccl::exception(std::string(__PRETTY_FUNCTION__) + " - is not implemented");
    return {};
}
ccl::event thread_device_group_ring_communicator::alltoall_impl(
    const ccl::vector_class<void*>& send_buf,
    const ccl::vector_class<void*>& recv_buf,
    size_t count,
    ccl::datatype dtype,
    const ccl::stream::impl_value_t& stream,
    const ccl::alltoall_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    throw ccl::exception(std::string(__PRETTY_FUNCTION__) + " - is not implemented");
    return {};
}

/* alltoallv */
ccl::event thread_device_group_ring_communicator::alltoallv_impl(
    const void* send_buf,
    const ccl::vector_class<size_t>& send_counts,
    void* recv_buf,
    const ccl::vector_class<size_t>& recv_counts,
    ccl::datatype dtype,
    const ccl::stream::impl_value_t& stream,
    const ccl::alltoallv_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    using namespace native;
    static constexpr ccl::group_split_type group_id = base_t::topology_type();
    static constexpr ccl::device_topology_type class_id = base_t::topology_class();

    if (!is_ready()) {
        throw ccl::exception(std::string(
            "Device communicator for group_id: " + ::to_string(group_id) +
            " is not ready yet. Not all сommunicators are created in group. Please create them before usage"));
    }

    int comm_rank = rank();
    size_t ring_index = 0;
    LOG_DEBUG("communicator for device idx: ",
              get_device_path(),
              ", rank idx: ",
              comm_rank,
              ", ring_index :",
              ring_index);

    size_t total_send_counts = std::accumulate(std::begin(send_counts), std::end(send_counts), 0);
    //TODO make const!
    ccl_buffer send_entry_buffer(const_cast<void**>(&send_buf),
                                 total_send_counts * ccl::get_datatype_size(dtype),
                                 0,
                                 ccl_buffer_type::INDIRECT);

    size_t total_recv_counts = std::accumulate(std::begin(recv_counts), std::end(recv_counts), 0);
    ccl_buffer recv_entry_buffer(
        &recv_buf, total_recv_counts * ccl::get_datatype_size(dtype), 0, ccl_buffer_type::INDIRECT);

    using community_t = typename device_community_container<class_id>::element_type;
    community_t community = device_community_impl.get_topology(ring_index);

    coll_param_gpu params(ccl_coll_alltoallv, dtype);

    return do_collective_op<group_id, class_id, l0_alltoallv_typed_entry>(
        communication_device,
        ctx,
        community,
        thread_id,
        this->get_native_context(),
        send_entry_buffer,
        send_counts.data(),
        total_send_counts,
        recv_entry_buffer,
        recv_counts.data(),
        total_recv_counts,
        params,
        stream);
}
ccl::event thread_device_group_ring_communicator::alltoallv_impl(
    const ccl::vector_class<void*>& send_buf,
    const ccl::vector_class<size_t>& send_counts,
    ccl::vector_class<void*> recv_buf,
    const ccl::vector_class<size_t>& recv_counts,
    ccl::datatype dtype,
    const ccl::stream::impl_value_t& stream,
    const ccl::alltoallv_attr& attr,

    const ccl::vector_class<ccl::event>& dep) {
    throw ccl::exception(std::string(__PRETTY_FUNCTION__) + " - is not implemented");
    return {};
}

/* bcast */
ccl::event thread_device_group_ring_communicator::broadcast_impl(
    void* buf,
    size_t count,
    ccl::datatype dtype,
    int root,
    const ccl::stream::impl_value_t& stream,
    const ccl::broadcast_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    using namespace native;

    static constexpr ccl::group_split_type group_id = base_t::topology_type();
    static constexpr ccl::device_topology_type class_id = base_t::topology_class();

    if (!is_ready()) {
        throw ccl::exception(std::string(
            "Device communicator for group_id: " + ::to_string(group_id) +
            " is not ready yet. Not all сommunicators are created in group. Please create them before usage"));
    }

    int comm_rank = rank();
    size_t ring_index = 0;
    LOG_DEBUG("communicator for device idx: ",
              get_device_path(),
              ", rank idx: ",
              comm_rank,
              ", ring_index :",
              ring_index);

    //TODO make const!
    ccl_buffer entry_buffer(
        &buf, count * ccl::get_datatype_size(dtype), 0, ccl_buffer_type::INDIRECT);

    using community_t = typename device_community_container<class_id>::element_type;
    community_t community = device_community_impl.get_topology(ring_index);

    coll_param_gpu params(ccl_coll_bcast, dtype);

    return do_collective_op<group_id, class_id, l0_bcast_typed_entry>(communication_device,
                                                                      ctx,
                                                                      community,
                                                                      thread_id,
                                                                      this->get_native_context(),
                                                                      entry_buffer,
                                                                      count,
                                                                      root,
                                                                      params,
                                                                      stream);
}

/* reduce */
ccl::event thread_device_group_ring_communicator::reduce_impl(
    const void* send_buf,
    void* recv_buf,
    size_t count,
    ccl::datatype dtype,
    ccl::reduction reduction,
    int root,
    const ccl::stream::impl_value_t& stream,
    const ccl::reduce_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    using namespace native;

    static constexpr ccl::group_split_type group_id = base_t::topology_type();
    static constexpr ccl::device_topology_type class_id = base_t::topology_class();

    if (!is_ready()) {
        throw ccl::exception(std::string(
            "Device communicator for group_id: " + ::to_string(group_id) +
            " is not ready yet. Not all сommunicators are created in group. Please create them before usage"));
    }

    int comm_rank = rank();
    size_t ring_index = 0;
    LOG_DEBUG("communicator for device idx: ",
              get_device_path(),
              ", rank idx: ",
              comm_rank,
              ", ring_index :",
              ring_index);

    //TODO make const!
    ccl_buffer send_entry_buffer(const_cast<void**>(&send_buf),
                                 count * ccl::get_datatype_size(dtype),
                                 0,
                                 ccl_buffer_type::INDIRECT);
    ccl_buffer recv_entry_buffer(
        &recv_buf, count * ccl::get_datatype_size(dtype), 0, ccl_buffer_type::INDIRECT);

    using community_t = typename device_community_container<class_id>::element_type;
    community_t community = device_community_impl.get_topology(ring_index);

    coll_param_gpu params(ccl_coll_reduce, dtype, reduction);

    return do_collective_op<group_id, class_id, l0_reduce_typed_entry>(communication_device,
                                                                       ctx,
                                                                       community,
                                                                       thread_id,
                                                                       this->get_native_context(),
                                                                       send_entry_buffer,
                                                                       recv_entry_buffer,
                                                                       count,
                                                                       reduction,
                                                                       root,
                                                                       params,
                                                                       stream);
}

/* reduce_scatter */
ccl::event thread_device_group_ring_communicator::reduce_scatter_impl(
    const void* send_buf,
    void* recv_buf,
    size_t recv_count,
    ccl::datatype dtype,
    ccl::reduction reduction,
    const ccl::stream::impl_value_t& stream,
    const ccl::reduce_scatter_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    using namespace native;

    static constexpr ccl::group_split_type group_id = base_t::topology_type();
    static constexpr ccl::device_topology_type class_id = base_t::topology_class();

    if (!is_ready()) {
        throw ccl::exception(std::string(
            "Device communicator for group_id: " + ::to_string(group_id) +
            " is not ready yet. Not all сommunicators are created in group. Please create them before usage"));
    }

    int comm_rank = rank();
    size_t ring_index = 0;
    LOG_DEBUG("communicator for device idx: ",
              get_device_path(),
              ", rank idx: ",
              comm_rank,
              ", ring_index :",
              ring_index);

    //TODO make const!
    ccl_buffer send_entry_buffer(const_cast<void**>(&send_buf),
                                 recv_count * ccl::get_datatype_size(dtype),
                                 0,
                                 ccl_buffer_type::INDIRECT);
    ccl_buffer recv_entry_buffer(
        &recv_buf, recv_count * ccl::get_datatype_size(dtype), 0, ccl_buffer_type::INDIRECT);

    using community_t = typename device_community_container<class_id>::element_type;
    community_t community = device_community_impl.get_topology(ring_index);

    coll_param_gpu params(ccl_coll_reduce_scatter, dtype, reduction);

    return do_collective_op<group_id, class_id, l0_reduce_scatter_typed_entry>(
        communication_device,
        ctx,
        community,
        thread_id,
        this->get_native_context(),
        send_entry_buffer,
        recv_entry_buffer,
        recv_count,
        params,
        stream);
}

/* sparse_allreduce */
ccl::event thread_device_group_ring_communicator::sparse_allreduce_impl(
    const void* send_ind_buf,
    size_t send_ind_count,
    const void* send_val_buf,
    size_t send_val_count,
    void* recv_ind_buf,
    size_t recv_ind_count,
    void* recv_val_buf,
    size_t recv_val_count,
    ccl::datatype index_dtype,
    ccl::datatype value_dtype,
    ccl::reduction reduction,
    const ccl::stream::impl_value_t& stream,
    const ccl::sparse_allreduce_attr& attr,
    const ccl::vector_class<ccl::event>& deps) {
    throw ccl::exception(std::string(__PRETTY_FUNCTION__) + " - is not implemented");
    return {};
}

COMM_INTERFACE_COLL_INSTANTIATION(thread_device_group_ring_communicator);
#ifdef CCL_ENABLE_SYCL
SYCL_COMM_INTERFACE_COLL_INSTANTIATION(thread_device_group_ring_communicator);
#endif // CCL_ENABLE_SYCL
