#ifndef INDEX_BLOCK_BUFFER_HPP_
#define INDEX_BLOCK_BUFFER_HPP_
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <atomic>
#include <algorithm>
#include <vector>
#include "diskAgent.hpp"

#define DISK_BLOCK_TIMES 4

struct IndexBlock{
    u_int64_t block_id; // ID of the block in memory pool
    u_int64_t write_pos; // Position in the disk to write
    char* buffer; // Data buffer of the block
};

template<class KeyType>
class __attribute__((packed)) Record{
public:
    KeyType key;
    u_int64_t value;
    bool operator<(const Record& other) const{
        return key < other.key;
    }
    bool operator==(const Record& other) const{
        return key == other.key;
    }
    bool operator!=(const Record& other) const{
        return key != other.key;
    }
    bool operator<=(const Record& other) const{
        return key <= other.key;
    }
    bool operator>(const Record& other) const{
        return key > other.key;
    }
    bool operator>=(const Record& other) const{
        return key >= other.key;
    }
};

class IndexBlockBuffer{
private:
    const u_int64_t total_block_num;
    const u_int64_t block_size;
    const u_int64_t buffer_size;
    const u_int64_t disk_block_num;
    const u_int64_t thread_num;

    char* buffer_blocks;
    u_int64_t* block_disk_id;
    std::vector<u_int64_t> disk_write_ids;
    std::vector<u_int64_t> block_check_ids;

    bool checkThread(u_int64_t block_check_id, u_int64_t rss_id) const {
        u_int64_t disk_write_id = this->disk_write_ids[rss_id];
        u_int64_t barrier_len = this->total_block_num * 2;
        u_int64_t disk_left_barrier_id = this->block_disk_id[block_check_id];
        u_int64_t disk_right_barrier_id = disk_left_barrier_id % this->disk_block_num;
        disk_left_barrier_id = (disk_right_barrier_id + this->disk_block_num - barrier_len) % this->disk_block_num;
        if( (disk_left_barrier_id < disk_right_barrier_id && (disk_write_id > disk_right_barrier_id || disk_write_id < disk_left_barrier_id)) ||
            (disk_left_barrier_id > disk_right_barrier_id && disk_write_id > disk_right_barrier_id && disk_write_id < disk_left_barrier_id)){
            return true;
        }
        return false;
    }
    bool directCheckThread(u_int64_t block_check_id, u_int64_t rss_id) const {
        u_int64_t disk_write_id = this->disk_write_ids[rss_id];
        u_int64_t barrier_len = this->total_block_num * 2;
        u_int64_t disk_left_barrier_id = this->block_disk_id[block_check_id];
        u_int64_t disk_right_barrier_id = disk_left_barrier_id % this->disk_block_num;
        disk_left_barrier_id = (disk_right_barrier_id + this->disk_block_num - barrier_len) % this->disk_block_num;
        if( (disk_left_barrier_id < disk_right_barrier_id && (disk_write_id >= disk_right_barrier_id || disk_write_id < disk_left_barrier_id)) ||
            (disk_left_barrier_id > disk_right_barrier_id && disk_write_id >= disk_right_barrier_id && disk_write_id < disk_left_barrier_id)){
            return true;
        }
        return false;
    }
public:
    IndexBlockBuffer(const u_int64_t total_block_num, const u_int64_t block_size, const u_int64_t disk_block_num, const u_int64_t thread_num):
        total_block_num(total_block_num), block_size(block_size), buffer_size(total_block_num*block_size), disk_block_num(disk_block_num), thread_num(thread_num){
        if(this->buffer_size & (this->buffer_size - 1)){
            printf("Index block buffer error: buffer size %lu is not power of 2!\n",this->buffer_size);
            throw std::runtime_error("buffer size wrong");
        }
        if(this->total_block_num * DISK_BLOCK_TIMES >= this->disk_block_num){
            printf("Index block buffer error: buffer num %lu is too large!\n",this->total_block_num);
            throw std::runtime_error("block buffer number wrong");
        }
        // one backup block
        this->buffer_blocks = (char*)mmap(nullptr, this->buffer_size + this->block_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        // printf("buffer size: %lu\n",this->buffer_size);
        if (this->buffer_blocks == MAP_FAILED){
            printf("Index block buffer error: mmap failed for blocks!\n");
            throw std::runtime_error("memory manager mmap failed");
        }

        this->block_disk_id = new u_int64_t[this->total_block_num];
        for(u_int64_t i=0;i<this->total_block_num;++i){
            this->block_disk_id[i] = i;
        }

        this->disk_write_ids = std::vector<u_int64_t>();
        this->block_check_ids = std::vector<u_int64_t>();
    }
    ~IndexBlockBuffer(){
        delete[] this->block_disk_id;
        munmap(this->buffer_blocks, this->buffer_size + this->block_size);
    }
    void Print(){
        for(u_int64_t i = 0; i<100; ++i){
            printf("%x",(u_int8_t)this->buffer_blocks[i]);
        }
    }
    u_int64_t addWriteThread(){
        u_int64_t id = this->disk_write_ids.size();
        if(id >= this->total_block_num){
            printf("Index block buffer error: too many written threads!\n");
            return std::numeric_limits<uint64_t>::max();
        }
        this->disk_write_ids.push_back(id);
        return id;
    }
    u_int64_t addCheckThread(){
        u_int64_t id = this->block_check_ids.size();
        if(id >= this->total_block_num){
            printf("Index block buffer error: too many written threads!\n");
            return std::numeric_limits<uint64_t>::max();
        }
        this->block_check_ids.push_back(id);
        return id;
    }
    bool writeBlock(const char* data, u_int64_t len, u_int64_t disk_pos, u_int64_t thread_id){
        u_int64_t block_id = (disk_pos / this->block_size) % this->total_block_num;
        u_int64_t disk_id = (disk_pos / this->block_size) % this->disk_block_num;

        u_int64_t block_offset = disk_pos % this->block_size;
        u_int64_t block_pos = disk_pos % this->buffer_size;


        if (this->block_disk_id[block_id] != disk_id){
            this->disk_write_ids[thread_id] = disk_id;
            return false;
        }

        if (block_offset + len >= this->block_size){
            block_id++;
            block_id %= this->total_block_num;
            disk_id++;
            disk_id %= this->disk_block_num;
            if (this->block_disk_id[block_id] != disk_id){
                this->disk_write_ids[thread_id] = disk_id;
                return false;
            }
        }

        if (block_pos + len > this->buffer_size){
            // printf("write block a: len %lu, block pos %lu, buffer size %lu\n",len,block_pos,this->buffer_size);
            u_int64_t tmp = this->buffer_size - block_pos;
            memcpy(this->buffer_blocks + block_pos, data, tmp);
            // printf("write block a continue.\n");
            memcpy(this->buffer_blocks, data + tmp, len - tmp);
            // printf("write block a done.\n");
            this->disk_write_ids[thread_id] = disk_id;
            return true;
        }

        // if(len == 4 && thread_id == 0){
        //     printf("%u.%u.%u.%u\n",(u_int8_t)data[0],(u_int8_t)data[1],(u_int8_t)data[2],(u_int8_t)data[3]);
        // }
        // printf("write block b: len %lu, block pos %lu, buffer size %lu\n",len,block_pos,this->buffer_size);
        memcpy(this->buffer_blocks + block_pos, data, len);
        // printf("write block b done.\n");
        this->disk_write_ids[thread_id] = disk_id;
        return true;
    }
    void sortIndex(u_int64_t start_pos, u_int64_t len, u_int64_t key_len){
        u_int64_t block_pos = start_pos % this->buffer_size;

        if (block_pos + len > this->buffer_size){
            memcpy(this->buffer_blocks + this->buffer_size, this->buffer_blocks, len + block_pos - this->buffer_size);
        }

        u_int64_t record_size = key_len + sizeof(uint64_t);
        u_int64_t num_records = len / record_size;

        if (key_len == 2){
            Record<u_int16_t>* record_list = (Record<u_int16_t>*)(this->buffer_blocks + block_pos);
            
            // for(u_int64_t i = 0; i<10;++i){
            //     printf("%u %lu\n",record_list[i].key, record_list[i].value);
            //     printf("port:%u pos:%lu\n",*(u_int16_t*)(this->buffer_blocks + block_pos + i * sizeof(Record<u_int16_t>)),*(u_int64_t*)(this->buffer_blocks + block_pos + sizeof(u_int16_t)));
            // }
            // printf("\n");
            std::sort(record_list, record_list + num_records);
            // for(u_int64_t i = 0; i<10;++i){
            //     printf("%u %lu\n",record_list[i].key, record_list[i].value);
            // }
        } else if (key_len == 4){
            Record<uint32_t>* record_list = (Record<uint32_t>*)(this->buffer_blocks + block_pos);
            // printf("ip:%u pos:%lu\n",*(u_int32_t*)(this->buffer_blocks + block_pos),*(u_int64_t*)(this->buffer_blocks + block_pos + sizeof(u_int32_t)));
            // for(u_int64_t i = 0; i<10;++i){
            //     printf("%u %lu\n",record_list[i].key, record_list[i].value);
            // }
            // printf("\n");
            std::sort(record_list, record_list + num_records);
            // for(u_int64_t i = 0; i<10;++i){
            //     printf("%u %lu\n",record_list[i].key, record_list[i].value);
            // }
        } else if (key_len == 16){
            Record<IPv6Address>* record_list = (Record<IPv6Address>*)(this->buffer_blocks + block_pos);
            // for(u_int64_t i = 0; i<10;++i){
            //     printf("%lu %lu\n",record_list[i].key.high, record_list[i].value);
            // }
            // printf("\n");
            std::sort(record_list, record_list + num_records);
            // for(u_int64_t i = 0; i<10;++i){
            //     printf("%lu %lu\n",record_list[i].key.high, record_list[i].value);
            // }
        } else {
            throw std::runtime_error("Unsupported key_len");
        }

        if (block_pos + len > this->buffer_size){
            memcpy(this->buffer_blocks, this->buffer_blocks + this->buffer_size, len + block_pos - this->buffer_size);
        }

    }
    u_int64_t checkBlock(u_int64_t thread_id) const{
        u_int64_t block_check_id = this->block_check_ids[thread_id];

        for(u_int64_t i = 0; i < this->thread_num; ++i){
            if(!this->checkThread(block_check_id, i)){
                return std::numeric_limits<uint64_t>::max();
            }
        }
        return block_check_id;
    }
    u_int64_t directCheckBlock(u_int64_t thread_id) const{
        u_int64_t block_check_id = this->block_check_ids[thread_id];

        for(u_int64_t i = 0; i < this->thread_num; ++i){
            if(this->directCheckThread(block_check_id, i)){
                return block_check_id;
            }
        }
        return std::numeric_limits<uint64_t>::max();
    }
    IndexBlock* getBlock(u_int64_t thread_id){
        IndexBlock* block = new IndexBlock();
        block->block_id = this->block_check_ids[thread_id];
        block->write_pos = this->block_disk_id[block->block_id];
        block->buffer = this->buffer_blocks + block->block_id * this->block_size;

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