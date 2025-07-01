#ifndef CHUNKPOOL_HPP_
#define CHUNKPOOL_HPP_
#include <iostream>
#include <atomic>
#include "utils.hpp"

struct Chunk {
    u_int8_t* data;
    std::atomic<Chunk*> next;
    const u_int32_t size;

    Chunk(u_int32_t _size):size(_size) {
        data = new u_int8_t[size];
        next.store(nullptr);
    }
};

class LockFreeChunkPool {
private:
    std::atomic<Chunk*> head;
    std::atomic<Chunk*> tail;
public:
    const u_int32_t chunk_size;
    LockFreeChunkPool(u_int32_t chunk_size = CHUNK_SIZE):chunk_size(chunk_size) {
        Chunk* dummy = new Chunk(chunk_size);  // dummy 节点
        head.store(dummy);
        tail.store(dummy);
    }
    
    ~LockFreeChunkPool() {
        Chunk* node = head.load();
        while (node) {
            Chunk* next = node->next.load();
            delete node;
            node = next;
        }
    }
    
    // 放回一个 chunk
    void deallocate(Chunk* chunk) {
        Chunk* new_node = chunk;
        Chunk* old_tail;
    
        while (true) {
            old_tail = tail.load(std::memory_order_acquire);
            Chunk* next = old_tail->next.load(std::memory_order_acquire);
            if (next == nullptr) {
                if (old_tail->next.compare_exchange_weak(next, new_node)) {
                    break;
                }
            } else {
                // 尾节点已落后，推进
                tail.compare_exchange_weak(old_tail, next);
            }
        }

        // 尝试推进 tail 指针
        tail.compare_exchange_weak(old_tail, new_node);
    }
    
    // 从池中取出一个 chunk，如果为空，返回 nullptr
    Chunk* allocate() {
        Chunk* old_head;
        while (true) {
            old_head = head.load(std::memory_order_acquire);
            Chunk* next = old_head->next.load(std::memory_order_acquire);
            if (next == nullptr) {
                return nullptr; // 队列空
            }
    
            if (head.compare_exchange_weak(old_head, next)) {
                return old_head;
            }
        }
    }
    
    // 预填充 chunk 数量
    void preload(u_int32_t count, u_int32_t chunk_size) {
        for (u_int32_t i = 0; i < count; ++i) {
            Chunk* chunk = new Chunk(chunk_size);
            deallocate(chunk);
        }
    }

};

class AggBuffer {
private:
    LockFreeChunkPool* pool;
    Chunk* head;
    Chunk* tail;
    u_int32_t total_length;
    u_int32_t current_chunk_used;
    const u_int32_t chunk_size;
public:
    AggBuffer(LockFreeChunkPool* pool) : pool(pool), chunk_size(pool->chunk_size) {
        this->head = this->tail = pool->allocate();
        if (!head) {
            throw std::runtime_error("Failed to allocate initial chunk");
            return;
        }
        this->head->next.store(nullptr);
        this->tail->next.store(nullptr);
        this->total_length = 0;
        this->current_chunk_used = 0;
    }
    
    ~AggBuffer() {
        Chunk* current = head;
        while (current) {
            Chunk* next = current->next.load();
            pool->deallocate(current);
            current = next;
        }
    }
    
    bool append(const uint8_t* data, u_int32_t len) {
        while (len > 0) {
            u_int32_t space = this->chunk_size - this->current_chunk_used;
            if (space >= len){
                std::memcpy(tail->data + this->current_chunk_used, data, len);
                this->current_chunk_used += len;
                this->total_length += len;
                break;
            }
            else {
                if (space > 0){
                    std::memcpy(tail->data + this->current_chunk_used, data, space);
                    this->total_length += space;
                    len -= space;
                }
                auto* new_chunk = pool->allocate();
                if (!new_chunk){
                    return false;
                }
                this->tail->next.store(new_chunk);
                this->tail = new_chunk;
                this->tail->next.store(nullptr);
                this->current_chunk_used = 0;
                data += space;
            }
        }
        return true;
    }

    u_int32_t length() const {
        return this->total_length;
    }

    Chunk* get_head() const {
        return this->head;
    }
};

#endif