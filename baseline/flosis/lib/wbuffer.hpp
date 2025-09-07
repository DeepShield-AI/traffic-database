#ifndef WBUFFER_HPP_
#define WBUFFER_HPP_
#include <cstdint>
#include <iostream>
#include <flowbuffer.hpp>
// #include "chunkpool.hpp"
#include "aggBuffer.hpp"

class WBuffer{
private:
    // Chunk* buffer;
    // LockFreeChunkPool* pool;
    const u_int64_t buffer_size;
    const u_int64_t block_size;
    const u_int64_t block_count;

    char* buffer;
    std::atomic_bool* signalBuffer;
    u_int64_t writingBlock;
    u_int64_t writePos;
    u_int64_t readingBlock;

    std::atomic_bool stop;
    
public:
    WBuffer(u_int64_t buffer_size, u_int64_t block_size):buffer_size(buffer_size), block_size(block_size),block_count(buffer_size/block_size){
        this->buffer = (char*)mmap(nullptr, this->buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (this->buffer == MAP_FAILED){
            printf("Pointer ring pool error: mmap failed for mem_pool!\n");
            throw std::runtime_error("memory manager mmap failed");
        }
        this->signalBuffer = new std::atomic_bool[block_count];
        for(u_int64_t i = 0;i<this->block_count;++i){
            this->signalBuffer[i] = false;
        }
        this->writingBlock = 0;
        this->writePos = 0;
        this->readingBlock = 0;
        this->stop = false;
    }
    ~WBuffer(){
        munmap(this->buffer,this->buffer_size);
    }
    void put(AggBuffer* aggBuffer){
        u_int64_t len = aggBuffer->getTotalLen();
        u_int64_t block_len = aggBuffer->getBlockSize();
        Chunk* head = aggBuffer->getHeadChunk();

        Chunk* current = head;
        u_int64_t remaining = len;
        while (current && remaining > 0){
            u_int64_t to_write = remaining < block_len ? remaining : block_len;
            while (this->writePos + to_write >= this->block_size){
                memcpy(this->buffer + this->writingBlock * this->block_size + this->writePos, current->data, this->block_size - this->writePos);
                while(this->signalBuffer[(this->writingBlock + 1) % this->block_count]){
                    printf("Wbuffer warning: memory pool run out of memory.\n");
                }
                this->signalBuffer[this->writingBlock] = true;
                this->writingBlock = (this->writingBlock + 1) % this->block_count;
                
                to_write -= this->block_size - this->writePos;
                remaining -= this->block_size - this->writePos;
                this->writePos = 0;
            }
            if(to_write != 0){
                memcpy(this->buffer + this->writingBlock * this->block_size + this->writePos, current->data, to_write);
            }
            remaining -= to_write;
            current = current->next;
            this->writePos += to_write;
        }
    }
    u_int64_t getBlockSize() const{
        return this->block_size;
    }
    char* get(){
        while (!this->signalBuffer[this->readingBlock]){
            if(this->stop){
                printf("wbuffer stop\n");
                break;
            }
        }

        char* ret = this->buffer + this->readingBlock * this->block_size;
        this->readingBlock++;
        return ret;
    }
    void asynchronousStop(){
        this->stop = true;
    }
};


#endif