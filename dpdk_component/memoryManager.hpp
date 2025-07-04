#ifndef MEMORY_MANAFER_HPP_
#define MEMORY_MANAGER_HPP_
#include <sys/mman.h>
#include "../dpdk_lib/pointerRingBuffer.hpp"
#include "../dpdk_lib/diskAgent.hpp"

class MemoryManager{
private:
    const u_int32_t block_size;
    const u_int64_t pool_size;
    char* memoryPool;

    PointerRingBuffer* block_ring;
    DiskAgent* disk_agent;

    std::atomic_bool stop;
public:
    MemoryManager(u_int32_t block_size, u_int64_t pool_size, PointerRingBuffer* block_ring, DiskAgent* disk_agent):
        block_size(block_size), pool_size(pool_size), block_ring(block_ring), disk_agent(disk_agent), stop(false) {
        if(this->pool_size % this->block_size != 0){
            printf("Memory manager error: pool size should be multiple of block size!");
            throw std::runtime_error("memory manager block size failed");
        }
        this->memoryPool = (char*)mmap(nullptr, this->pool_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (this->memoryPool == MAP_FAILED){
            printf("Memory manager error: mmap failed for disk metas!\n");
            throw std::runtime_error("memory manager mmap failed");
        }
    }
    ~MemoryManager(){
        munmap(this->memoryPool, this->pool_size);
    }
};


#endif