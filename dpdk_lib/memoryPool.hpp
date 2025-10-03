#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <atomic>
#include "util.hpp"
#include "indexBlockBuffer.hpp"

struct NodeMeta{
    u_int64_t data_block_id;
    u_int64_t start_offset;
    bool dirty;
};


class MemoryPool{
private:
    const u_int64_t capacity;
    const u_int64_t list_len;
    const u_int64_t unit_len;

    // const u_int64_t disk_block_num;
    // const u_int64_t barraier_size;

    char* buffer;
    u_int64_t allocate_pos;
    NodeMeta* allocated_list;
    u_int64_t list_tail;
    u_int64_t list_head;
    // u_int64_t empty_tail_len;

    void addNodeMeta(u_int64_t data_block_id, u_int64_t pool_offset){
        if(this->list_head == this->list_tail){
            printf("Memory pool error: alloacted list node run out!\n");
            while(this->list_head == this->list_tail);
        }
        this->allocated_list[this->list_tail].data_block_id = data_block_id;
        this->allocated_list[this->list_tail].start_offset = pool_offset;
        this->allocated_list[this->list_tail].dirty = false;

        u_int64_t new_tail = (this->list_tail + 1) % this->list_len;
        this->list_tail = new_tail;
    }

public:
    MemoryPool(char* buffer,u_int64_t capacity, u_int64_t list_len, u_int64_t uint_len):
        buffer(buffer),capacity(capacity),list_len(list_len),unit_len(uint_len){
        // this->buffer = (char*)mmap(nullptr, this->capacity, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        // if (this->buffer == MAP_FAILED){
        //     printf("Memory pool error: mmap failed for blocks!\n");
        //     throw std::runtime_error("memory manager mmap failed");
        // }
        if (capacity % unit_len != 0){
            printf("Memory pool error: capacity not aligned with unit length!\n");
            throw std::runtime_error("Memory pool capacity not aligned with unit length");
        }
        this->allocate_pos = 0;
        this->allocated_list = new NodeMeta[list_len];
        this->allocated_list[0] = {
            .data_block_id = std::numeric_limits<uint64_t>::max(),
            .start_offset = std::numeric_limits<uint64_t>::max(),
            .dirty = false,
        };
        this->list_head = 0;
        this->list_tail = 1;
        // this->empty_tail_len = 0;
    }
    ~MemoryPool(){
        delete[] this->allocated_list;
        // munmap(buffer,this->capacity);
    }

    void Print(){
        for(u_int64_t i = 0; i<100; ++i){
            printf("%x",(u_int8_t)this->buffer[i]);
        }
    }

    char* allocate(u_int64_t len, u_int64_t disk_block_id){
        
        while(this->allocated_list[list_head].dirty){
            if((this->list_head + 1) % this->list_len == this->list_tail){
                break;
            }
            // printf("clear node meta %lu\n",this->allocated_list[list_head].data_block_id);
            this->list_head = (this->list_head + 1) % this->list_len;
        }

        if (this->allocate_pos + len > this->capacity){
            // this->empty_tail_len = this->capacity - this->allocate_pos;
            // printf("new turn.\n");
            this->allocate_pos = 0;
        }
        u_int64_t end = this->allocate_pos + len;
        // while (this->allocated_list[list_head].dirty){
        //     this->list_head = (this->list_head + 1) % this->list_len;
        // }
        auto barrier = this->allocated_list[list_head].start_offset;
        // first allocation
        if (barrier == std::numeric_limits<uint64_t>::max() && this->allocate_pos + len <= this->capacity){
            // printf("first\n");
            this->allocated_list[list_head].data_block_id = disk_block_id;
            this->allocated_list[list_head].start_offset = this->allocate_pos;
            char* p = this->buffer + this->allocate_pos;
            this->allocate_pos = end;
            return p;
        }
        while(barrier >= this->allocate_pos && barrier < end){
            printf("Memory buffer pool warning: memory overhead.\n");
            barrier = this->allocated_list[list_head].start_offset;
        }

        if (disk_block_id != this->allocated_list[(this->list_tail + this->list_len - 1) % this->list_len].data_block_id){
            this->addNodeMeta(disk_block_id, this->allocate_pos);
        }
    
        char* p = this->buffer + this->allocate_pos;
        this->allocate_pos = end;
        return p;
    }
    u_int64_t getListLen()const{
        return (this->list_tail + this->list_len - this->list_head) % this->list_len;
    }
    u_int64_t getLenOfDiskID(u_int64_t disk_block_id)const{
        u_int64_t tmp_head = this->list_head;
        u_int64_t total_len = 0;
        for(u_int64_t count = 0; count < this->list_len; ++count){
            if (tmp_head == this->list_tail){
                break;
            }
            if(this->allocated_list[tmp_head].data_block_id == disk_block_id){
                u_int64_t start_offset = this->allocated_list[tmp_head].start_offset;
                // printf("start offset: %lu\n",start_offset);
                u_int64_t end_offset = this->allocate_pos;
                if (tmp_head + 1 != this->list_tail){
                    end_offset = this->allocated_list[(tmp_head + 1) % this->list_len].start_offset;
                    // printf("end offset: %lu\n",end_offset);
                }
                if (end_offset > start_offset){
                    total_len += end_offset - start_offset;
                }else{
                    total_len += this->capacity - start_offset + end_offset;
                }
                // printf("start offset: %lu,end offset: %lu-%lu\n",start_offset,end_offset,this->allocated_list[(tmp_head + 1) % this->list_len].start_offset);
                break;
            }    
            tmp_head = (tmp_head + 1) % this->list_len;
        }
        return total_len;
    }
    void writeToBuffer(u_int64_t disk_block_id, u_int64_t len, IndexBlockBuffer* buffer, u_int64_t thread_id, u_int64_t disk_pos){
        u_int64_t tmp_head = this->list_head;
        for(u_int64_t count = 0; count < this->list_len; ++count){
            if (tmp_head == this->list_tail){
                break;
            }
            if(this->allocated_list[tmp_head].data_block_id == disk_block_id){
                u_int64_t start_offset = this->allocated_list[tmp_head].start_offset;
                
                if (start_offset + len < this->capacity){
                    // printf("write a\n");
                    buffer->writeBlock(this->buffer + start_offset, len, disk_pos, thread_id);
                    // printf("write a done\n");
                }else{
                    // printf("write b\n");
                    buffer->writeBlock(this->buffer + start_offset, this->capacity - start_offset, disk_pos, thread_id);
                    disk_pos += this->capacity - start_offset;
                    // printf("write b continue\n");
                    buffer->writeBlock(this->buffer, len + start_offset - this->capacity, disk_pos, thread_id);
                    // printf("write b done\n");
                }
                break;
            }    
            tmp_head = (tmp_head + 1) % this->list_len;
        }
    }
    void recycle(u_int64_t disk_block_id){
        u_int64_t tmp_head = this->list_head;
        // printf("recycle node meta %lu\n",disk_block_id);
        for(u_int64_t count = 0; count < this->list_len; ++count){
            if (tmp_head == this->list_tail){
                return;
            }
            if(this->allocated_list[tmp_head].data_block_id == disk_block_id){
                this->allocated_list[tmp_head].dirty = true;
                return;
            }    
            tmp_head = (tmp_head + 1) % this->list_len;
            // count++;
            // if (count >= this->barraier_size){
            //     break;
            // }
        }
    }
};

#endif