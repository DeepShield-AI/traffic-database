#ifndef DATA_BLOCK_BUFFER_HPP_
#define DATA_BLOCK_BUFFER_HPP_
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <atomic>
#include <algorithm>
// #include "util.hpp"

// #define PAGE_SIZE 1024
// #define LEAST_BLOCK_NUM 3
#define DISK_BLOCK_TIMES 4

// struct BlockCheckThreadMata{
//     u_int64_t block_check_id;
//     u_int64_t block_written_id;
// };

struct DiskBlock{
    // u_int32_t rss_id; // RSS ID of the packet handling thread
    u_int64_t block_id; // ID of the block in memory pool
    u_int64_t write_pos; // Position in the disk to write
    // u_int64_t last_write_pos; // Position of last block with the same RSS ID
    u_int64_t start_time; // Start time of packets in block
    u_int64_t end_time; // End time of packets in block
    char* buffer; // Data buffer of the block
};

class DataBlockBuffer{
private:
    const u_int64_t total_block_num; // total number of blocks in buffer
    const u_int64_t block_size; // size of each block
    const u_int64_t buffer_size;
    const u_int64_t disk_block_num;
    const u_int64_t rss_num;
    u_int64_t delayed_block_num;

    char* buffer_blocks; // blocks
    // bool* finish_flags; // block written has been finished by RSS threads
    // bool* dirty_flags; // block is written and can not be modified
    u_int64_t* block_disk_id; // which disk block this buffer block corresponds to
    u_int64_t* start_times;
    u_int64_t* end_times;
    // std::vector<BlockCheckThreadMata> thread_metas;
    std::vector<u_int64_t> disk_write_ids;
    std::vector<u_int64_t> block_check_ids;

    bool checkRSS(u_int64_t block_check_id, u_int64_t rss_id) const {
        u_int64_t disk_write_id = this->disk_write_ids[rss_id];
        u_int64_t barrier_len = this->total_block_num * 2;
        u_int64_t disk_left_barrier_id = this->block_disk_id[block_check_id];
        u_int64_t disk_right_barrier_id = (disk_left_barrier_id + this->delayed_block_num) % this->disk_block_num;
        disk_left_barrier_id = (disk_right_barrier_id + this->disk_block_num - barrier_len) % this->disk_block_num;
        if(disk_write_id > disk_right_barrier_id || disk_write_id < disk_left_barrier_id){
            return true;
        }
        return false;
    }
    bool directCheckRSS(u_int64_t block_check_id, u_int64_t rss_id) const{
        u_int64_t disk_write_id = this->disk_write_ids[rss_id];
        u_int64_t barrier_len = this->total_block_num * 2;
        u_int64_t disk_left_barrier_id = this->block_disk_id[block_check_id];
        u_int64_t disk_right_barrier_id = disk_left_barrier_id % this->disk_block_num;
        disk_left_barrier_id = (disk_right_barrier_id + this->disk_block_num - barrier_len) % this->disk_block_num;
        if(disk_write_id > disk_right_barrier_id || disk_write_id < disk_left_barrier_id){
            return true;
        }
        return false;
    }
public:
    DataBlockBuffer(const u_int64_t total_block_num, const u_int64_t block_size, const u_int64_t disk_block_num, const u_int64_t rss_num, u_int64_t delayed_block_num):
        total_block_num(total_block_num), block_size(block_size), buffer_size(total_block_num*block_size), disk_block_num(disk_block_num), rss_num(rss_num), delayed_block_num(delayed_block_num){
        if(this->buffer_size & (this->buffer_size - 1)){
            printf("Data block buffer error: buffer size %u is not power of 2!\n",this->buffer_size);
            throw std::runtime_error("buffer size wrong");
        }
        if(this->total_block_num * DISK_BLOCK_TIMES >= this->disk_block_num){
            printf("Data block buffer error: buffer num %u is too large!\n",this->total_block_num);
            throw std::runtime_error("block buffer number wrong");
        }
        this->buffer_blocks = (char*)mmap(nullptr, this->buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (this->buffer_blocks == MAP_FAILED){
            printf("Data block buffer error: mmap failed for blocks!\n");
            throw std::runtime_error("memory manager mmap failed");
        }
        // this->finish_flags = new bool[this->total_block_num * this->rss_num];
        // for(u_int64_t i=0;i<this->total_block_num * this->rss_num;++i){
        //     this->finish_flags[i] = false;
        // }
        // this->dirty_flags = new bool[this->total_block_num];
        // for(u_int64_t i=0;i<this->total_block_num;++i){
        //     this->dirty_flags[i] = false;
        // }
        this->block_disk_id = new u_int64_t[this->total_block_num];
        for(u_int64_t i=0;i<this->total_block_num;++i){
            this->block_disk_id[i] = i;
        }
        // this->thread_metas = std::vector<BlockCheckThreadMata>();
        this->start_times = new u_int64_t[this->total_block_num];
        for(u_int64_t i=0;i<this->total_block_num;++i){
            this->start_times[i] = std::numeric_limits<uint64_t>::max();
        }
        this->end_times = new u_int64_t[this->total_block_num]();
        this->disk_write_ids = std::vector<u_int64_t>();
        this->block_check_ids = std::vector<u_int64_t>();
    }
    ~DataBlockBuffer(){
        // delete[]  this->finish_flags;
        delete[] this->start_times;
        delete[] this->end_times;
        delete[] this->block_disk_id;
        munmap(this->buffer_blocks, this->buffer_size);
    }
    u_int64_t addWriteThread(){
        u_int64_t id = this->disk_write_ids.size();
        if(id >= this->total_block_num){
            printf("Data block buffer error: too many written threads!\n");
            return std::numeric_limits<uint64_t>::max();
        }
        this->disk_write_ids.push_back(id);
        return id;
    }
    u_int64_t addCheckThread(){
        // u_int64_t id = this->thread_metas.size();
        u_int64_t id = this->block_check_ids.size();
        if(id >= this->total_block_num){
            printf("Data block buffer error: too many check threads!\n");
            return std::numeric_limits<uint64_t>::max();
        }
        // BlockCheckThreadMata meta = {
        //     .block_check_id = id,
        //     .block_written_id = id,
        // };
        // this->thread_metas.push_back(meta);
        this->block_check_ids.push_back(id);
        return id;
    }
    bool writeBlock(const char* data, u_int64_t len, u_int64_t disk_pos, u_int64_t thread_id, bool new_data, u_int64_t ts){
        u_int64_t block_id = disk_pos / this->total_block_num;
        u_int64_t disk_id = disk_pos / this->disk_block_num;

        u_int64_t block_offset = disk_pos % this->block_size;
        u_int64_t block_pos = disk_pos % this->buffer_size;


        if (this->block_disk_id[block_id] != disk_id){
            if(new_data){
                this->disk_write_ids[thread_id] = disk_id;
            }
            return false;
        }

        if (block_offset + len >= this->block_size){
            if(new_data) this->end_times[block_id] = ts;
            block_id++;
            block_id %= this->total_block_num;
            disk_id++;
            disk_id %= this->disk_block_num;
            if (this->block_disk_id[block_id] != disk_id){
                if(new_data){
                    this->disk_write_ids[thread_id] = disk_id;
                }
                return false;
            }
            if(new_data) this->start_times[block_id] = std::min(this->start_times[block_id],ts);
        }

        if (block_pos + len > this->buffer_size){
            u_int64_t tmp = this->block_size - block_pos;
            memcpy(this->buffer_blocks + block_pos, data, tmp);
            memcpy(this->buffer_blocks, data + tmp, len - tmp);
            // this->disk_write_ids[thread_id] = disk_id;
            if(new_data){
                this->disk_write_ids[thread_id] = disk_id;
            }
            return true;
        }

        memcpy(this->buffer_blocks + block_pos, data, len);
        if(new_data){
            this->disk_write_ids[thread_id] = disk_id;
        }
        return true;
    }
    u_int64_t checkBlock(u_int64_t thread_id) const{
        u_int64_t block_check_id = this->block_check_ids[thread_id];
        // u_int64_t real_check_id = (block_check_id + this->delayed_block_num) % this->total_block_num;
        for(u_int64_t i = 0; i < this->rss_num; ++i){
            if(!this->checkRSS(block_check_id, i)){
                return std::numeric_limits<uint64_t>::max();
            }
        }
        return block_check_id;
    }
    u_int64_t directGetBlockID(u_int64_t thread_id) const{
        u_int64_t block_check_id = this->block_check_ids[thread_id];
        for(u_int64_t i = 0; i < this->rss_num; ++i){
            if(this->directCheckRSS(block_check_id,i)){
                return block_check_id;
            }
        }
        return std::numeric_limits<uint64_t>::max();
    }
    // u_int64_t getDiskID(u_int64_t block_check_id) const{
    //     return this->block_disk_id[block_check_id];
    // }
    DiskBlock* getBlock(u_int64_t thread_id){
        DiskBlock* block = new DiskBlock();
        block->block_id = this->block_check_ids[thread_id];
        block->start_time = this->start_times[block->block_id];
        block->end_time = this->end_times[block->block_id];
        block->write_pos = this->block_disk_id[block->block_id];
        block->buffer = this->buffer_blocks + block->block_id * this->block_size;
        // u_int64_t block_check_id = this->block_check_ids[thread_id];

        this->block_check_ids[thread_id] += this->block_check_ids.size();
        this->block_check_ids[thread_id] %= this->total_block_num;
        
        return block;
    }
    void recycleBlock(u_int64_t block_id){
        u_int64_t disk_id = this->block_disk_id[block_id];
        disk_id += this->total_block_num;
        disk_id %= this->disk_block_num;
        this->block_disk_id[block_id] = disk_id;
    }
};
#endif