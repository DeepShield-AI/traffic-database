#ifndef DATA_BLOCK_BUFFER_HPP_
#define MDATA_BLOCK_BUFFER_HPP_
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <atomic>
#include <libaio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "util.hpp"

#define PAGE_SIZE 1024
#define LEAST_BLOCK_NUM 3

struct BlockCheckThreadMata{
    u_int64_t block_check_id;
    u_int64_t block_written_id;
};

class DataBlockBuffer{
private:
    const u_int64_t total_block_num; // total number of blocks in buffer
    const u_int64_t block_size; // size of each block
    const u_int64_t buffer_size;
    const u_int64_t disk_block_num;
    const u_int64_t rss_num;

    char* buffer_blocks; // blocks
    bool* finish_flags; // block written has been finished by RSS threads
    std::vector<BlockCheckThreadMata> thread_metas;
public:
    DataBlockBuffer(const u_int64_t total_block_num, const u_int64_t block_size, const u_int64_t disk_block_num, const u_int64_t rss_num):
        total_block_num(total_block_num), block_size(block_size), buffer_size(total_block_num*block_size), disk_block_num(disk_block_num), rss_num(rss_num){
        if(this->buffer_size & (this->buffer_size - 1)){
            printf("Data block buffer error: buffer size %u is not power of 2!\n",this->buffer_size);
            throw std::runtime_error("buffer size wrong");
        }
        this->buffer_blocks = (char*)mmap(nullptr, this->buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (this->buffer_blocks == MAP_FAILED){
            printf("Data block buffer error: mmap failed for blocks!\n");
            throw std::runtime_error("memory manager mmap failed");
        }
        this->finish_flags = new bool[this->total_block_num * this->rss_num];
        for(u_int64_t i=0;i<this->total_block_num * this->rss_num;++i){
            this->finish_flags[i] = false;
        }
        this->thread_metas = std::vector<BlockCheckThreadMata>();
    }
    ~DataBlockBuffer(){
        delete[]  this->finish_flags;
        munmap(this->buffer_blocks, this->buffer_size);
    }
    u_int64_t addWrittenThread(){
        u_int64_t id = this->thread_metas.size();
        if(id >= this->total_block_num){
            printf("Data block buffer error: too many written threads!\n");
            return std::numeric_limits<uint64_t>::max();
        }
        BlockCheckThreadMata meta = {
            .block_check_id = id,
            .block_written_id = id,
        };
        this->thread_metas.push_back(meta);
        return id;
    }
    bool writeBlock(const char* data, u_int64_t len, u_int64_t write_pos){

    }
    u_int64_t checkBlock(u_int64_t thread_id){

    }
    void recycleBlock(u_int64_t block_id){

    }
};
#endif