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

#include "ze_primitives.hpp"
#include "common/global/global.hpp"

#include <unordered_map>

namespace ccl {

namespace ze {

class ze_init {
public:
    static ze_init& instance() {
        static ze_init init;
        return init;
    }

private:
    ze_init() {
        LOG_DEBUG("l0 initialization");
        ZE_CALL(zeInit(ZE_INIT_FLAG_GPU_ONLY));
    }
};

template <ccl_coll_type coll_type>
class ze_module_loader {
public:
    void get(ze_device_handle_t device, ze_context_handle_t context, ze_module_handle_t* module) {
        std::lock_guard<std::mutex> lock(mutex);
        bool success = false;

        auto module_it = cache.find(device);
        if (module_it != cache.end()) {
            LOG_DEBUG("load module from cache");
            *module = module_it->second;
            success = true;
        }
        else {
            LOG_DEBUG("load module from file and push it to cache");
            std::string modules_dir = ccl::global_data::env().comm_kernels_path;
            // TODO: remove
            if (modules_dir.empty()) {
                std::string ccl_root = getenv("CCL_ROOT");
                CCL_THROW_IF_NOT(!ccl_root.empty(),
                                 "incorrect comm kernels path, CCL_ROOT not found!");
                modules_dir = ccl_root + "/lib/kernels/";
            }
            ccl::ze::load_module(modules_dir, get_file_name(), device, context, module);
            success = cache.insert({ device, *module }).second;
        }
        CCL_THROW_IF_NOT(success == true, "module loading error");
    }

    static ze_module_loader<coll_type>& instance() {
        static ze_module_loader<coll_type> loader;
        return loader;
    }

private:
    std::unordered_map<ze_device_handle_t, ze_module_handle_t> cache;
    std::mutex mutex;

    ze_module_loader() = default;

    constexpr const char* get_file_name() {
        // TODO: add another
        switch (coll_type) {
            case ccl_coll_allreduce: return "ring_allreduce.spv";
            default: return "";
        }
    }

    void cleanup() {
        LOG_DEBUG("cleanup cache");
        for (auto& item : cache) {
            ZE_CALL(zeModuleDestroy(item.second));
        }
    }

    ~ze_module_loader() {
        cleanup();
    }
};

} // namespace ze
} // namespace ccl
