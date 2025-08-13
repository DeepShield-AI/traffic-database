#ifndef MEMORY_MANAGER_HPP_
#define MEMORY_MANAGER_HPP_
#include <sys/mman.h>
// #include "../dpdk_lib/pointerRingBuffer.hpp"
#include "../dpdk_lib/diskAgent.hpp"
#include "../dpdk_lib/dataBlockbuffer.hpp"
#include "../dpdk_lib/indexBlockBuffer.hpp"
#include "../dpdk_lib/util.hpp"

class MemoryManager{
private:
    const u_int32_t block_size;
    // const u_int64_t pool_size;
    // char* memoryPool;

    // u_int64_t memory_offset;
    // u_int64_t memory_size;

    // PointerRingBuffer* block_ring;
    // DataBlockBuffer* block_buffer;
    void* block_buffer;
    AgentType agent_type;
    std::vector<DiskAgent*> disk_agents;

    std::atomic_bool stop;

    bool bind_core;
    u_int32_t core_id;

    void bindCore();
    // void allocate_blocks();
public:
    MemoryManager(u_int32_t block_size, void* block_buffer, AgentType agent_type):
        block_size(block_size), block_buffer(block_buffer), agent_type(agent_type) {
        // if(this->pool_size % this->block_size != 0){
        //     printf("Memory manager error: pool size should be multiple of block size!");
        //     throw std::runtime_error("memory manager block size failed");
        // }
        // this->memoryPool = (char*)mmap(nullptr, this->pool_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        // if (this->memoryPool == MAP_FAILED){
        //     printf("Memory manager error: mmap failed for disk metas!\n");
        //     throw std::runtime_error("memory manager mmap failed");
        // }
        this->disk_agents = std::vector<DiskAgent*>();
        this->bind_core = false;
        this->core_id = 0;
        this->stop = true;
    }
    ~MemoryManager(){
        // munmap(this->memoryPool, this->pool_size);
    }
    void addAgent(DiskAgent* agent);
    void setBindCore(u_int32_t core_id);
    int run();
    void asynchronousStop();
};


#endif