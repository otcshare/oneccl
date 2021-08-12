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
#include "sycl_base.hpp"

using namespace std;
using namespace sycl;

int main(int argc, char *argv[]) {
    const size_t count = 10 * 1024 * 1024;

    int i = 0;
    int size = 0;
    int rank = 0;

    ccl::init();

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    atexit(mpi_finalize);

    queue q;

    buf_allocator<int> allocator(q);

    /* create kvs */
    ccl::shared_ptr_class<ccl::kvs> kvs;
    ccl::kvs::address_type main_addr;
    if (rank == 0) {
        kvs = ccl::create_main_kvs();
        main_addr = kvs->get_address();
        MPI_Bcast((void *)main_addr.data(), main_addr.size(), MPI_BYTE, 0, MPI_COMM_WORLD);
    }
    else {
        MPI_Bcast((void *)main_addr.data(), main_addr.size(), MPI_BYTE, 0, MPI_COMM_WORLD);
        kvs = ccl::create_kvs(main_addr);
    }

    /* create communicator */
    auto dev = ccl::create_device(q.get_device());
    auto ctx = ccl::create_context(q.get_context());
    auto comm = ccl::create_communicator(size, rank, dev, ctx, kvs);

    /* create stream */
    auto stream = ccl::create_stream(q);

    /* create buffers */
    const int num_iters = 10;

    // Allocate enough memory to check results on each iteration without
    // additional memory transfer between host and device.
    int *check_buf = allocator.allocate(count * num_iters, usm::alloc::shared);

    std::vector<ccl::event> ccl_events;
    std::vector<sycl::event> sycl_events;

    // Store allocated mem ptrs to free them later
    std::vector<int *> ptrs;

    for (int i = 0; i < num_iters; ++i) {
        std::cout << "Running iteration " << i << std::endl;
        auto send_buf = allocator.allocate(count, usm::alloc::device);
        auto recv_buf = allocator.allocate(count, usm::alloc::device);

        ptrs.push_back(send_buf);
        ptrs.push_back(recv_buf);

        /* open buffers and modify them on the device side */
        auto e = q.submit([&](auto &h) {
            h.parallel_for(count, [=](auto id) {
                // Make the value differ on each iteration
                send_buf[id] = (i + 1) * (rank + 1);
                recv_buf[id] = -1;
            });
        });

        /* do not wait completion of kernel and provide it as dependency for operation */
        vector<ccl::event> deps;
        deps.push_back(ccl::create_event(e));

        /* invoke allreduce */
        auto attr = ccl::create_operation_attr<ccl::allreduce_attr>();
        ccl_events.push_back(ccl::allreduce(
            send_buf, recv_buf, count, ccl::reduction::sum, comm, stream, attr, deps));

        /* submit result checking after allreduce completion */
        sycl_events.push_back(q.submit([&](auto &h) {
            h.depends_on(ccl_events[i].get_native());
            h.parallel_for(count, [=](auto id) {
                if (recv_buf[id] != (i + 1) * (size * (size + 1) / 2)) {
                    check_buf[count * i + id] = -1;
                }
            });
        }));
    }

    // Wait on the last generated events from each iteration
    for (auto &&e : sycl_events)
        e.wait();

    for (int *ptr : ptrs) {
        allocator.deallocate(ptr);
    }

    // Make sure there is no exceptions in the queue
    if (!handle_exception(q))
        return -1;

    /* Check if we have an error on some iteration */
    {
        for (i = 0; i < count * num_iters; i++) {
            if (check_buf[i] == -1) {
                cout << "FAILED\n";
                break;
            }
        }
        if (i == count * num_iters) {
            cout << "PASSED\n";
        }
    }

    return 0;
}
