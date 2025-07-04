#ifndef DISK_MANAGER_HPP_
#define DISK_MANAGER_HPP_

#include <vector>
#include <atomic>
#include "../dpdk_lib/memoryBuffer.hpp"
#include "../dpdk_lib/diskAgent.hpp"
#include "../dpdk_lib/diskBuffer.hpp"
#include "../dpdk_lib/pointerRingBuffer.hpp"

class DiskManager {
private:
    // std::vector<MemoryBuffer*> buffers;
    PointerRingBuffer* block_ring;
    DiskAgent* agent;
    DiskBuffer* disk_buffer;

    // std::vector<u_int32_t> checkID;
    std::atomic_bool stop;
    // u_int32_t testID;
    bool runUnit();
public:
    DiskManager(u_int32_t id, PointerRingBuffer* block_ring, DiskAgent* agent, DiskBuffer* disk_buffer):
        block_ring(block_ring), agent(agent), disk_buffer(disk_buffer){
        this->stop = false;
        // this->testID = id;
    }
    ~DiskManager()=default;
    void addBlock(DiskBlock* block);
    void setMeta(u_int64_t buffer_id, DiskBlock* block);
    int run();
    void asynchronousStop();
};

#endif