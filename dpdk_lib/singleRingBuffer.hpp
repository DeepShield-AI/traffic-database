#ifndef SINGLERINGBUFFER_HPP_
#define SINGLERINGBUFFER_HPP_
#include <iostream>
#include <unistd.h>
#include <atomic>
#include <sys/mman.h>
#include "header.hpp"
#include "skipList.hpp"
#include "util.hpp"
#define CACHE_LINE_LEN 64

// write Not covered, read covered
class PointerRingBuffer{
private:
    const u_int32_t capacity_;
    // std::atomic_uint64_t writePos;
    // std::atomic_uint64_t readPos;
    // std::atomic_uint_fast64_t writePos;
    // std::atomic_uint_fast64_t readPos;
    alignas(CACHE_LINE_LEN) u_int64_t writePos;
    char writepadding[CACHE_LINE_LEN - sizeof(uint64_t)];
    alignas(CACHE_LINE_LEN) u_int64_t readPos;
    char readpadding[CACHE_LINE_LEN - sizeof(uint64_t)];

    void** pointers;
    // std::atomic_bool* signalBuffer_;
    
    // std::atomic_bool has_begin;
    // std::atomic_uint32_t readThreadCount;
    // std::atomic_uint32_t writeThreadCount;

    std::atomic_bool stop;

    bool isPowerOfTwo(u_int32_t n) {
        return (n & (n - 1)) == 0;
    }
public:
    PointerRingBuffer(u_int32_t capacity):capacity_(capacity){
        if(this->capacity_ & (this->capacity_ - 1)){
            printf("PointerRingBuffer error: capacity %u is not power of 2!\n",capacity);
            this->pointers = nullptr;
            // this->signalBuffer_ = nullptr;
            return;
        }
        this->pointers = new void*[this->capacity_];
        // this->pointers = (void**)mmap(nullptr, this->capacity_ * sizeof(void*), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        // if (this->pointers == MAP_FAILED){
        //     printf("Pointer ring buffer error: mmap failed for blocks!\n");
        //     throw std::runtime_error("memory manager mmap failed");
        // }
        for(u_int32_t i = 0;i<this->capacity_;++i){
            this->pointers[i] = nullptr;
        }
        // this->signalBuffer_ = new std::atomic_bool[this->capacity_];
        // for(u_int32_t i = 0;i<this->capacity_;++i){
        //     this->signalBuffer_[i] = false;
        // }
        this->writePos = 0;
        this->readPos = 0;
        
        // this->readThreadCount = 0;
        // this->writeThreadCount = 0;
        // this->has_begin = false;
        this->stop = false;
    }
    ~PointerRingBuffer(){
        // if(this->readThreadCount || this->writeThreadCount){
        //     std::cout << "Pointer ring buffer warning: destroy while it is used by certain thread." <<std::endl;
        // }
        delete this->pointers;
        // delete this->signalBuffer_;
    }
    bool put(void* data){
        // if(this->pointers == nullptr){
        //     printf("Pointer ring buffer error: put with null pointers!\n");
        //     return false;
        // }

        u_int64_t pos = this->writePos;
        pos &= this->capacity_ - 1;
        
        while(this->writePos == this->capacity_ - 1 + this->readPos){
            printf("ring buffer wait\n");
        } // wait util not writed

        u_int64_t real_pos = ((pos & ((this->capacity_ >> 3) - 1)) << 3) + pos / (this->capacity_ >> 3);

        this->pointers[real_pos] = data;
        this->writePos ++;

        return true;
    }
    void* get(){
        // if(this->pointers == nullptr){
        //     printf("Pointer ring buffer error: get with null pointers!\n");
        //     return nullptr;
        // }
        
        u_int64_t pos = this->readPos;
        pos &= this->capacity_ - 1;

        while(this->writePos == this->readPos){
            if(this->stop){
                return nullptr;
            }
        } // wait util writed

        u_int64_t real_pos = ((pos & ((this->capacity_ >> 3) - 1)) << 3) + pos / (this->capacity_ >> 3);

        void* data = this->pointers[real_pos];
        this->readPos ++;
 
        return data;
    }
    bool initSucceed()const{
        return this->pointers != nullptr;
    }
    void asynchronousStop(){
        this->stop = true;
    }
};
#endif