/*
 *  (c) Copyright 2016-2017 Hewlett Packard Enterprise Development Company LP.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As an exception, the copyright holders of this Library grant you permission
 *  to (i) compile an Application with the Library, and (ii) distribute the
 *  Application containing code generated by the Library and added to the
 *  Application during this compilation process under terms of your choice,
 *  provided you also meet the terms and conditions of the Application license.
 *
 */

#include <unistd.h> // sleep
#include <list>
#include <random>
#include <limits>
#include <vector>
#include <thread>
#include <chrono>

#include <gtest/gtest.h>
#include "nvmm/memory_manager.h"
#include "test_common/test.h"

using namespace nvmm;

// random number and string generator
std::random_device r;
std::default_random_engine e1(r());
uint64_t rand_uint64(uint64_t min=0, uint64_t max=std::numeric_limits<uint64_t>::max())
{
    std::uniform_int_distribution<uint64_t> uniform_dist(min, max);
    return uniform_dist(e1);
}

// regular free
TEST(EpochZoneHeap, Free)
{
    PoolId pool_id = 1;
    size_t size = 128*1024*1024LLU; // 128 MB

    MemoryManager *mm = MemoryManager::GetInstance();
    Heap *heap = NULL;

    // create a heap
    EXPECT_EQ(ID_NOT_FOUND, mm->FindHeap(pool_id, &heap));
    EXPECT_EQ(NO_ERROR, mm->CreateHeap(pool_id, size));
    EXPECT_EQ(ID_FOUND, mm->CreateHeap(pool_id, size));

    // get the heap
    EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
    EXPECT_EQ(NO_ERROR, heap->Open());

    // allocate & free
    GlobalPtr ptr = heap->Alloc(sizeof(int));
    heap->Free(ptr);

    // allocate again, because of immediate free, the new ptr should be the same as the previous ptr
    GlobalPtr ptr1 = heap->Alloc(sizeof(int));
    EXPECT_EQ(ptr, ptr1);
    heap->Free(ptr1);

    // destroy the heap
    EXPECT_EQ(NO_ERROR, heap->Close());
    delete heap;
    EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
    EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
}

// delayed free
TEST(EpochZoneHeap, DelayedFree)
{
    PoolId pool_id = 1;
    size_t size = 128*1024*1024LLU; // 128 MB

    MemoryManager *mm = MemoryManager::GetInstance();
    EpochManager *em = EpochManager::GetInstance();
    Heap *heap = NULL;

    // create a heap
    EXPECT_EQ(ID_NOT_FOUND, mm->FindHeap(pool_id, &heap));
    EXPECT_EQ(NO_ERROR, mm->CreateHeap(pool_id, size));
    EXPECT_EQ(ID_FOUND, mm->CreateHeap(pool_id, size));

    // get the heap
    EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
    EXPECT_EQ(NO_ERROR, heap->Open());


    EpochCounter e1;
    GlobalPtr ptr1;

    // allocate & delayed free
    {
        EpochOp op(em);
        e1 = op.reported_epoch();
        std::cout << "first epoch " << e1 << std::endl;
        ptr1 = heap->Alloc(op, sizeof(int));
        heap->Free(op, ptr1);
        // allocate again, because of delayed free, the new ptr should be different from t he
        // previous ptr
        GlobalPtr ptr2 = heap->Alloc(op, sizeof(int));
        EXPECT_NE(ptr1, ptr2);
        heap->Free(op, ptr2);
    }

    // wait a few epoches and make sure the background thread picks up this chunk and frees it
    EpochCounter e2;
    while(1)
    {
        {
            // Begin epoch in a new scope block so that we exit the epoch when 
            // we out of scope and don't block others when we then sleep. 
	    {
            	EpochOp op(em);
            	e2 = op.reported_epoch();
	    }
            if (e2-e1>=3 && e2%5 == (e1+3)%5) {
                std::cout << "sleeping at epoch " << e2 << std::endl;
                sleep(1); // making sure the background thread wakes up in this epoch
                break;
            }
        }
    }

    while(1)
    {
        {
            EpochOp op(em);
            EpochCounter e3 = op.reported_epoch();
            if (e3>e2) {
                break;
            }
        }
    }

    // now the ptr that was delayed freed must have been actually freed
    {
        EpochOp op(em);
        std::cout << "final epoch " << op.reported_epoch() << std::endl;
        GlobalPtr ptr2 = heap->Alloc(op, sizeof(int));
        EXPECT_EQ(ptr1, ptr2);
        heap->Free(ptr2);
    }

    // destroy the heap
    EXPECT_EQ(NO_ERROR, heap->Close());
    delete heap;
    EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
    EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
}

// merge
TEST(EpochZoneHeap, Merge)
{
    PoolId pool_id = 1;
    size_t size = 128*1024*1024LLU; // 128 MB

    MemoryManager *mm = MemoryManager::GetInstance();
    Heap *heap = NULL;

    // create a heap
    EXPECT_EQ(ID_NOT_FOUND, mm->FindHeap(pool_id, &heap));
    EXPECT_EQ(NO_ERROR, mm->CreateHeap(pool_id, size));
    EXPECT_EQ(ID_FOUND, mm->CreateHeap(pool_id, size));

    // get the heap
    EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
    EXPECT_EQ(NO_ERROR, heap->Open());


    // in unit of 64-byte:
    // [0, 8) has been allocated to the header
    // [4096, 8192) has been allocated to the merge bitmap

    uint64_t min_obj_size = heap->MinAllocSize();

    // merge at levels < max_zone_level-2
    // allocate 64 byte x 24, covering [8, 32)
    GlobalPtr ptr[24];
    for(int i=0; i<24; i++) {
        ptr[i]= heap->Alloc(min_obj_size);
    }
    // free 64 byte x 24
    for(int i=0; i<24; i++) {
        heap->Free(ptr[i]);
    }

    // before merge, allocate 1024 bytes
    GlobalPtr new_ptr = heap->Alloc(16*min_obj_size);
    EXPECT_EQ(32*min_obj_size, new_ptr.GetOffset());

    // merge
    heap->Merge();

    // after merge, allocate 1024 bytes
    new_ptr = heap->Alloc(16*min_obj_size);
    EXPECT_EQ(16*min_obj_size, new_ptr.GetOffset());


    // merge at the last 3 levels
    // allocate 16MB x 7
    for(int i=0; i<7; i++) {
        ptr[i]= heap->Alloc(262144*min_obj_size);
    }
    // free 16MB x 7
    for(int i=0; i<24; i++) {
        heap->Free(ptr[i]);
    }

    // before merge, allocate 64MB
    new_ptr = heap->Alloc(1048576*min_obj_size);
    EXPECT_EQ(0UL, new_ptr.GetOffset());

    // merge
    heap->Merge();

    // after merge, allocate 64MB
    new_ptr = heap->Alloc(1048576*min_obj_size);
    EXPECT_EQ(1048576*min_obj_size, new_ptr.GetOffset());

    // destroy the heap
    EXPECT_EQ(NO_ERROR, heap->Close());
    delete heap;
    EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
    EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
}

void AllocFree(Heap *heap, int cnt) {
    std::cout << "Thread " << std::this_thread::get_id() << " started" << std::endl;
    std::list<GlobalPtr> ptrs;
    for(int i=0; i<cnt; i++) {
        if(rand_uint64(0,1)==1) {
            GlobalPtr ptr = heap->Alloc(rand_uint64(0,1024*1024));
            if(ptr)
                ptrs.push_back(ptr);
        }
        else {
            if(!ptrs.empty()) {
                GlobalPtr ptr = ptrs.front();
                ptrs.pop_front();
                heap->Free(ptr);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    for(auto ptr:ptrs) {
        heap->Free(ptr);
    }
    std::cout << "Thread " << std::this_thread::get_id() << " ended" << std::endl;
}

// merge and concurrent alloc and free
TEST(EpochZoneHeap, MergeAllocFree)
{
    PoolId pool_id = 1;
    size_t size = 1024*1024*1024LLU; // 1024 MB
    int thread_cnt = 16;
    int loop_cnt = 1000;

    MemoryManager *mm = MemoryManager::GetInstance();
    Heap *heap = NULL;

    // create a heap
    EXPECT_EQ(ID_NOT_FOUND, mm->FindHeap(pool_id, &heap));
    EXPECT_EQ(NO_ERROR, mm->CreateHeap(pool_id, size));
    EXPECT_EQ(ID_FOUND, mm->CreateHeap(pool_id, size));

    // get the heap
    EXPECT_EQ(NO_ERROR, mm->FindHeap(pool_id, &heap));
    EXPECT_EQ(NO_ERROR, heap->Open());

    // start the threads
    std::vector<std::thread> workers;
    for(int i=0; i<thread_cnt; i++) {
        workers.push_back(std::thread(AllocFree, heap, loop_cnt));
    }

    for(int i=0; i<5; i++) {
        heap->Merge();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (auto &worker:workers) {
        if(worker.joinable())
            worker.join();
    }

    heap->Merge();

    // destroy the heap
    EXPECT_EQ(NO_ERROR, heap->Close());
    delete heap;
    EXPECT_EQ(NO_ERROR, mm->DestroyHeap(pool_id));
    EXPECT_EQ(ID_NOT_FOUND, mm->DestroyHeap(pool_id));
}

int main(int argc, char** argv)
{
    InitTest(nvmm::trace, false);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
