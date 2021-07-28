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

#include <ze_api.h>

namespace ccl {
namespace ze {

// rule level zero calls serialization
enum serialize_mode {
    ze_serialize_none = 0, // no locking or blocking
    ze_serialize_lock = 1, // locking around each ZE_CALL
    ze_serialize_block = 2, // blocking ZE calls
};

// class provides the serialization of level zero calls
class ze_call {
private:
    // mutex that is used for total serialization
    static std::mutex mutex;

public:
    ze_call();
    ~ze_call();
    void do_call(ze_result_t ze_result, const char* ze_name);
};

//host synchronize primitives
template <typename T>
ze_result_t zeHostSynchronize(T handle);
template <typename T, typename Func>
ze_result_t zeHostSynchronizeImpl(Func sync_func, T handle) {
    return sync_func(handle, std::numeric_limits<uint64_t>::max());
}

} // namespace ze
} // namespace ccl
