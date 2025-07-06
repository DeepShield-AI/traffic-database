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
    // const std::string disk_name;
    const u_int64_t disk_size;
    const u_int32_t block_size;
    const u_int64_t block_num;
    const u_int32_t total_manager_count;
    int disk_fd;
    u_int32_t threadID;
    u_int32_t rss_count;
    // u_int32_t ring_depth;
    // u_int32_t ring_idle_time;

    // std::vector<MemoryBuffer*> buffers;
    PointerRingBuffer* block_ring;
    std::vector<DiskAgent*> agents;
    DiskBuffer* disk_buffer;
    u_int32_t* last_rss_id_positions;

    // std::vector<u_int32_t> checkID;
    
    u_int64_t writePos;
    std::atomic_bool stop;

    bool bind_core;
    u_int32_t core_id;

    // u_int32_t testID;
    bool runUnit();
    void setMeta(u_int64_t buffer_id, DiskBlock* block);
    void addBlock(DiskBlock* block);
public:
    DiskManager(u_int64_t disk_size, u_int32_t block_size, u_int32_t total_manager_count,int disk_fd, u_int32_t id, PointerRingBuffer* block_ring, DiskBuffer* disk_buffer, u_int32_t rss_count):
        disk_size(disk_size),block_size(block_size),block_num(disk_size/block_size),total_manager_count(total_manager_count), disk_fd(disk_fd), threadID(id),block_ring(block_ring), disk_buffer(disk_buffer),rss_count(rss_count){
        // this->disk_fd = open(this->disk_name.c_str(), O_DIRECT | O_RDWR);
        // if (this->disk_fd < 0) {
        //     printf("Disk manager error: failed to open disk %s!\n", this->disk_name.c_str());
        //     throw std::runtime_error("Disk open failed");
        // }
        // for(u_int32_t i = 0; i < agents_num; ++i) {
        //     DiskAgent* agent = new DiskAgent(disk_size, block_size, disk_fd, ring_depth, ring_idle_time);
        //     this->agents.push_back(agent);
        // }
        this->agents = std::vector<DiskAgent*>();
        this->last_rss_id_positions = new u_int32_t[rss_count];
        for(u_int32_t i = 0;i<rss_count;++i){
            this->last_rss_id_positions[i] = this->block_num;
        }
        this->writePos = 0;
        this->stop = true;

        this->bind_core = false;
        this->core_id = 0;
    }
    ~DiskManager(){
        delete[] this->last_rss_id_positions;
    }
    void addAgent(DiskAgent* agent);
    void bindCore(u_int32_t core_id);
    int run();
    void asynchronousStop();
};

#endif