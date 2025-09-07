#ifndef AGGBUFFER_HPP_
#define AGGBUFFER_HPP_

#include <cstring>

#include "pointerRingPool.hpp"

struct Chunk{
    char* data;
    Chunk* next;
};

class AggBuffer {
private:
    PointerRingPool* pool;
    Chunk* head;
    Chunk* write_tail;
    u_int64_t block_len;
    u_int64_t total_len;
public:
    AggBuffer(PointerRingPool* pool):pool(pool){
        char* block = this->pool->allocate();
        while (block == nullptr){
            block = this->pool->allocate();
        }
        this->head = new Chunk;
        this->head->data = block;
        this->head->next = nullptr;
        this->block_len = this->pool->getBlockSize();
        this->total_len = 0;
        this->write_tail = this->head;
    }
    ~AggBuffer(){
        Chunk* current = head;
        while (current!=nullptr){
            Chunk* next = current->next;
            this->pool->free(current->data);
            delete current;
            current = next;
        }
    }
    bool write(char* data, u_int64_t len){
        u_int64_t write_pos = this->total_len % this->block_len;
        if (write_pos + len < this->block_len){
            memcpy(this->write_tail->data + write_pos, data, len);
            this->total_len += len;
            return true;
        }
        memcpy(this->write_tail->data + write_pos, data, this->block_len - write_pos);

        Chunk* new_chunk = new Chunk;
        char* block = this->pool->allocate();
        while (block == nullptr){
            block = this->pool->allocate();
        }
        new_chunk->data = block;
        new_chunk->next = nullptr;
        this->write_tail->next = new_chunk;
        this->write_tail = new_chunk;
        if (write_pos + len != this->block_len){
            memcpy(this->write_tail->data, data + (this->block_len - write_pos), len - (this->block_len - write_pos));
        }
        this->total_len += len;
        return true;
    }
    u_int64_t getTotalLen() const{
        return this->total_len;
    }
    u_int64_t getBlockSize() const{
        return this->block_len;
    }
    Chunk* getHeadChunk() const{
        return this->head;
    }
    void output(char* dest){
        Chunk* current = head;
        u_int64_t remaining = this->total_len;
        while (current && remaining > 0){
            u_int64_t to_write = remaining < this->block_len ? remaining : this->block_len;
            memcpy(dest, current->data, to_write);
            remaining -= to_write;
            current = current->next;
        }
    }
};

#endif