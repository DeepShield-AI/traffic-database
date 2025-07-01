#ifndef WBUFFER_HPP_
#define WBUFFER_HPP_
#include <cstdint>
#include <iostream>
#include <flowbuffer.hpp>
#include "chunkpool.hpp"

class WBuffer{
private:
    Chunk* buffer;
    LockFreeChunkPool* pool;
    u_int32_t writePos;
    const u_int32_t buffer_size;
public:
    WBuffer(LockFreeChunkPool* _pool):pool(_pool),buffer_size(_pool->chunk_size), writePos(0){
        this->buffer = pool->allocate();
        if (!this->buffer) {
            throw std::runtime_error("Failed to allocate WBuffer");
        }
    }
    ~WBuffer(){
        pool->deallocate(this->buffer);
    }
    u_int32_t put(Chunk* chunk, u_int32_t offset, u_int32_t len){
        if (!chunk) {
            printf("Wbuffer error: Chunk is null!\n");
            return 0;
        }
        if (this->writePos + len < this->buffer_size){
            memcpy(this->buffer->data + this->writePos,chunk->data + offset,len);
            this->writePos += len;
            return len;
        }
        u_int32_t space = this->buffer_size - this->writePos;
        memcpy(this->buffer->data + this->writePos, chunk->data + offset, space);
        this->writePos += space;
        return space;
    }
    Chunk* getChunk() const{
        return this->buffer;
    }
    bool UpdateChunk(){
        Chunk* current = this->buffer;
        this->buffer = pool->allocate();
        pool->deallocate(current);
    }
};


#endif