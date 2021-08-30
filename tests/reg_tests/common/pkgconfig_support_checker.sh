#!/bin/bash
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

#########################################################################################
#CCL# Title         : Simple pkg-config support checker                                 #
#CCL# Tracker ID    : INFRA-1459                                                        #
#CCL#                                                                                   #
#CCL# Implemented in: Intel(R) Collective Communication Library 2021.4 (Gold) Update  4 #
#########################################################################################

SCRIPT_DIR=$(cd $(dirname "${BASH_SOURCE}") && pwd -P)
ROOT_DIR="$(dirname "${SCRIPT_DIR}")"
BASENAME="`basename $0 .sh`"
TEST_LOG="${BASENAME}.log"
BINFILE="${BASENAME}"

${SCRIPT_DIR}/${BINFILE} > ${TEST_LOG} 2>&1
rc=$?

if [[ $rc -ne 0 ]]; then
    echo "fail"
    exit 1
else
    rm -rf ${BINFILE} ${TEST_LOG}
    echo "Pass"
fi

