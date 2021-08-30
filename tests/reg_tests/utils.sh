function check_impi() {
#
# Copyright 2016-2020 Intel Corporation
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
    if [[ -z "$I_MPI_ROOT" ]] ; then
        echo "Error: \$I_MPI_ROOT is not set. Please source vars.sh"
        exit 1
    fi
}

function check_ccl() {
    if [[ -z "${CCL_ROOT}" ]]; then
        echo "Error: \$CCL_ROOT is not set. Please source vars.sh"
        exit 1
    fi
}
