#ifndef INDEX_PERSISTER_HPP
#define INDEX_PERSISTER_HPP
#include <bit>
#include <cstdint>
#include <bitset>
// #include "../dpdk_lib/skipListRecycle.hpp"
#include "../dpdk_lib/indexBuffer.hpp"
#include "../dpdk_lib/indexBlockBuffer.hpp"
#include "../dpdk_lib/diskBuffer.hpp"
#include "../dpdk_lib/memoryPool.hpp"

class IndexPersister{
private:
    const u_int64_t disk_size;
    const u_int64_t block_size;
    const u_int64_t block_num;
    // const u_int64_t skiplist_check_roll;

    // u_int64_t threshold;

    // std::vector<SkipList*>* skiplists;

    IndexBuffer* indexBuffer;
    IndexBlockBuffer* indexBlockBuffer;
    DiskBuffer* diskBuffer;
    // std::vector<MemoryPool*>* memoryPools;
    // std::atomic_uint64_t* skiplistCheckID;
    std::atomic_uint64_t* diskWritePos;

    u_int64_t index_buffer_thread_id;
    u_int64_t index_block_buffer_thread_id;

    std::atomic_bool stop;

    bool bind_core;
    u_int32_t core_id;

    u_int64_t bit_ceil(u_int32_t x);

    void bindCore(u_int32_t cpu);

    IndexBufferMeta* checkAndGetMeta();
    void persistMeta(IndexBufferMeta* meta);
    void clearMeta(IndexBufferMeta* meta);
    
public:
    IndexPersister(u_int64_t disk_size, u_int64_t block_size, IndexBuffer* indexBuffer, IndexBlockBuffer* indexBlockBuffer, DiskBuffer* diskBuffer, std::atomic_uint64_t* diskWritePos, bool bind_core = false, u_int32_t core_id = 0):
        disk_size(disk_size),block_size(block_size),block_num(disk_size/block_size),indexBuffer(indexBuffer),indexBlockBuffer(indexBlockBuffer),diskBuffer(diskBuffer),diskWritePos(diskWritePos),bind_core(bind_core),core_id(core_id){
        if(this->disk_size & (this->disk_size - 1)){
            printf("Index persister error: block size %lu is not power of 2!\n",this->disk_size);
            throw std::runtime_error("Unsupport block size");
        }
        this->stop = true;
        this->index_buffer_thread_id = this->indexBuffer->addCheckThread();
        this->index_block_buffer_thread_id = this->indexBlockBuffer->addWriteThread();
        // this->skiplistCheckID->store(0);
        // this->diskWritePos->store(0);
    }
    ~IndexPersister()= default;
    // void setThreadID(u_int64_t threadID);
    int run();
    void asynchronousStop();
};



#endif