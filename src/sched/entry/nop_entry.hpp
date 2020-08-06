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

#include "sched/entry/entry.hpp"

class nop_entry : public sched_entry {
public:
    static constexpr const char* class_name() noexcept {
        return "NOOP";
    }

    nop_entry(ccl_sched* sched) : sched_entry(sched) {}

    void start() override {}

    const char* name() const override {
        return class_name();
    }
};
