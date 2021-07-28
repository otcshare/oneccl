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

#ifdef CCL_ENABLE_SYCL

#include "sched/entry/copy/copy_helper.hpp"
#include "sched/entry/entry.hpp"

#include <CL/sycl.hpp>

class sycl_copy_entry : public sched_entry {
public:
    static constexpr const char* class_name() noexcept {
        return "SYCL_COPY";
    }

    bool is_gpu_entry() const noexcept override {
        return true;
    }

    sycl_copy_entry() = delete;
    sycl_copy_entry(ccl_sched* sched,
                    copy_direction direction,
                    ccl_buffer in_buf,
                    ccl_buffer out_buf,
                    size_t count,
                    const ccl_datatype& dtype,
                    const ccl_stream* stream,
                    bool is_sycl_buf = false,
                    size_t offset = 0)
            : sched_entry(sched),
              direction(direction),
              in_buf(in_buf),
              out_buf(out_buf),
              count(count),
              dtype(dtype),
              stream(stream),
              is_sycl_buf(is_sycl_buf),
              offset(offset),
              copier(sycl_copier(direction, in_buf, out_buf, count, dtype, is_sycl_buf, offset)) {}

    void start() override {
        LOG_DEBUG(class_name(), ": in_buf ", in_buf, ", out_buf ", out_buf, ", count ", count);

        copier.set_queue(((ccl_stream*)stream)->get_native_stream(sched->queue->get_idx()));
        ccl_tuple_for_each_indexed<ccl_sycl_buffer_one_dim_types>(copier);
        status = ccl_sched_entry_status_started;
    }

    void update() override {
        if (copier.is_completed()) {
            status = ccl_sched_entry_status_complete;
        }
    }

    const char* name() const override {
        return class_name();
    }

protected:
    void dump_detail(std::stringstream& str) const override {
        ccl_logger::format(str,
                           "direction ",
                           to_string(direction),
                           ", dtype ",
                           ccl::global_data::get().dtypes->name(dtype),
                           ", count ",
                           count,
                           ", in_buf ",
                           in_buf,
                           ", out_buf ",
                           out_buf,
                           ", stream ",
                           stream->to_string(),
                           ", is_sycl_buf ",
                           is_sycl_buf,
                           ", offset ",
                           offset,
                           "\n");
    }

private:
    copy_direction direction;
    ccl_buffer in_buf;
    ccl_buffer out_buf;
    size_t count;
    ccl_datatype dtype;
    const ccl_stream* stream;
    bool is_sycl_buf;
    size_t offset;
    sycl_copier copier;
};

#endif // CCL_ENABLE_SYCL
