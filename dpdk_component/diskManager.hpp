#ifndef DISK_MANAGER_HPP_
#define DISK_MANAGER_HPP_

#include <vector>
#include <atomic>
#include "../dpdk_lib/memoryBuffer.hpp"
#include "../dpdk_lib/diskAgent.hpp"
#include "../dpdk_lib/diskBuffer.hpp"

class DiskManager {
private:
    std::vector<MemoryBuffer*> buffers;
    std::vector<DiskAgent*> agents;
    DiskBuffer* diskBuffer;

    std::vector<u_int32_t> checkID;
    std::atomic_bool stop;
    u_int32_t testID;
    bool runUnit();
public:
    DiskManager(u_int32_t id, DiskBuffer* diskBuffer):diskBuffer(diskBuffer){
        this->buffers = std::vector<MemoryBuffer*>();
        this->agents = std::vector<DiskAgent*>();
        this->checkID = std::vector<u_int32_t>();
        this->stop = false;
        this->testID = id;
    }
    ~DiskManager()=default;
    void addBuffer(MemoryBuffer* buffer, DiskAgent* agent);
    void setBuffer(u_int64_t buffer_id, MemoryBuffer* buffer);
    int run();
    void asynchronousStop();
};

#endif