#ifndef INDEX_PERSISTER_HPP
#define INDEX_PERSISTER_HPP
#include "../dpdk_lib/skipListRecycle.hpp"
#include "../dpdk_lib/indexBlockBuffer.hpp"

class IndexPersister{
private:
    const u_int64_t disk_size;
    const u_int64_t block_size;
    const u_int64_t block_num;

    std::vector<SkipList*>* skiplists;
    IndexBlockBuffer* buffer;
    std::atomic_uint64_t* skiplistCheckID;
    std::atomic_uint64_t* diskWritePos;

    std::atomic_bool stop;

    bool bind_core;
    u_int32_t core_id;

    u_int64_t thread_id;

    void bindCore(u_int32_t cpu);
public:
    IndexPersister(u_int64_t disk_size, u_int64_t block_size, std::vector<SkipList*>* skiplists, IndexBlockBuffer* buffer, std::atomic_uint64_t* skiplistCheckID, std::atomic_uint64_t* diskWritePos, bool bind_core, u_int32_t core_id):
        disk_size(disk_size),block_size(block_size),block_num(disk_size/block_size),skiplists(skiplists),buffer(buffer),skiplistCheckID(skiplistCheckID),diskWritePos(diskWritePos),bind_core(bind_core),core_id(core_id){
        if(this->block_num & (this->block_size - 1)){
            printf("Index persister error: block size %u is not power of 2!\n",this->block_size);
            return;
        }
        this->stop = true;
        this->skiplistCheckID->store(0);
        this->diskWritePos->store(0);
    }
    ~IndexPersister()= default;
    void setThreadID(u_int64_t threadID);
    int run();
    void asynchronousStop();
};



#endif