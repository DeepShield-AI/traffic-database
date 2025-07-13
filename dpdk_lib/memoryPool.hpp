#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <atomic>
#include "util.hpp"

struct NodeMeta{
    u_int64_t data_block_id;
    u_int64_t start_offset;
};


class MemoryPool{
private:
    const u_int64_t capacity;
    const u_int64_t list_len;

    char* buffer;
    u_int64_t allocate_pos;
    NodeMeta* allocated_list;
    u_int64_t list_tail;
    u_int64_t list_head;

    void addNodeMeta(u_int64_t data_block_id, u_int64_t pool_offset){
        if(this->list_head == this->list_tail){
            printf("Memory pool error: alloacted list node run out!\n");
            while(this->list_head == this->list_tail);
        }
        this->allocated_list[list_tail].data_block_id = data_block_id;
        this->allocated_list[list_tail].start_offset = pool_offset;

        u_int64_t new_tail = (list_tail + 1) % this->list_len;
        this->list_tail = new_tail;
    }

public:
    MemoryPool(u_int64_t capacity, u_int64_t list_len):
        capacity(capacity),list_len(list_len){
        this->buffer = (char*)mmap(nullptr, this->capacity, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (this->buffer == MAP_FAILED){
            printf("Memory pool error: mmap failed for blocks!\n");
            throw std::runtime_error("memory manager mmap failed");
        }
        this->allocate_pos = 0;
        this->allocated_list = new NodeMeta[list_len];
        this->allocated_list[0] = {
            .data_block_id = 0,
            .start_offset = 0,
        };
        this->list_head = 0;
        this->list_tail = 1;
    }
    ~MemoryPool(){
        delete[] this->allocated_list;
        munmap(buffer,this->capacity);
    }

    char* allocate(u_int64_t len, u_int64_t disk_block_id){
        if (this->allocate_pos + len > this->capacity){
            this->allocate_pos = 0;
        }
        u_int64_t end = this->allocate_pos + len;
        auto barrier = this->allocated_list[list_head].start_offset;
        while(barrier > this->allocate_pos && barrier <= end){
            barrier = this->allocated_list[list_head].start_offset;
        }

        if (disk_block_id != this->allocated_list[(list_tail + this->list_len) % this->list_len].data_block_id){
            this->addNodeMeta(disk_block_id,end);
        }
    
        char* p = this->buffer + this->allocate_pos;
        this->allocate_pos += len;
        return p;
    }
    u_int64_t getListLen()const{
        return (this->list_tail + this->list_len - this->list_head) % this->list_len;
    }
    void recycle(u_int64_t disk_block_id){
        if(this->allocated_list[list_head].data_block_id != disk_block_id){
            return;
        }
        u_int64_t new_head = (this->list_head + 1) % this->list_len;
        this->list_head = new_head;
    }
};

#endif