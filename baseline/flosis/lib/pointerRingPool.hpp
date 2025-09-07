#ifndef POINTERRINGPOOL_HPP_
#define POINTERRINGPOOL_HPP_
#include <iostream>
#include <unistd.h>
#include <atomic>
#include <sys/mman.h>

class PointerRingPool{
private :
    const u_int64_t mem_size;
    const u_int64_t block_size;
    const u_int64_t block_count;
    std::atomic_uint_fast64_t alloPos;
    std::atomic_uint_fast64_t freePos;

    char* mem_pool;
    char** blocks;
    std::atomic_bool* signalBuffer;

    // std::atomic_uint32_t alloThreadCount;
    // std::atomic_uint32_t freeThreadCount;

    // std::atomic_bool stop;

    bool isPowerOfTwo(u_int32_t n) {
        return (n & (n - 1)) == 0;
    }
public:
    PointerRingPool(u_int64_t _mem_size,u_int64_t _block_size):
        mem_size(_mem_size),block_size(_block_size),block_count(_mem_size/_block_size){
        if(this->isPowerOfTwo(this->block_count)==false){
            printf("PointerRingPool error: block count %lu is not power of 2!\n",this->block_count);
            this->mem_pool = nullptr;
            this->blocks = nullptr;
            this->signalBuffer = nullptr;
            return;
        }
        this->mem_pool = (char*)mmap(nullptr, this->mem_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (this->mem_pool == MAP_FAILED){
            printf("Pointer ring pool error: mmap failed for mem_pool!\n");
            throw std::runtime_error("memory manager mmap failed");
        }
        this->blocks = new char*[this->block_count];
        for(u_int64_t i = 0;i<this->block_count;++i){
            this->blocks[i] = this->mem_pool + i * this->block_size;
        }
        this->signalBuffer = new std::atomic_bool[this->block_count];
        for(u_int64_t i = 0;i<this->block_count;++i){
            this->signalBuffer[i] = true;
        }
        this->alloPos = 0;
        this->freePos = 0;
        // this->alloThreadCount = 0;
        // this->freeThreadCount = 0;
        // this->stop = false;
    }
    ~PointerRingPool(){
        // if(this->alloThreadCount || this->freeThreadCount){
        //     std::cout << "Pointer ring buffer warning: destroy while it is used by certain thread." <<std::endl;
        // }
        delete this->signalBuffer;
        delete this->blocks;
        munmap(this->mem_pool,this->mem_size);
    }
    bool free(char* block){
        if(block == nullptr){
            printf("Pointer ring pool error: free with invalid block id!\n");
            return false;
        }

        u_int64_t pos= this->freePos++;
        pos %= this->block_count;

        if (this->signalBuffer[pos]){
            printf("Pointer ring pool error: free with unallocated buffer!\n");
            return false;
        }

        this->blocks[pos] = block;
        this->signalBuffer[pos] = true;
        return true;
    }
    char* allocate(){
        u_int64_t pos = this->alloPos++;
        pos %= this->block_count;
        if(!this->signalBuffer[pos]){
            printf("Pointer ring pool warning: memory pool run out of memory.\n");
            return nullptr;
        }

        char* data = this->blocks[pos];
        this->signalBuffer[pos] = false;

        return data;
    }
    u_int64_t getBlockSize() const {
        return this->block_size;
    }
};


#endif