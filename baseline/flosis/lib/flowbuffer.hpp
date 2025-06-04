#ifndef FLOWBUFFER_HPP_
#define FLOWBUFFER_HPP_
#include <iostream>
#include <atomic>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <cstring>

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

struct ParsedPacket {
    bool is_upstream;         // 上行方向
    const uint8_t* data;
    u_int16_t data_length;
    const uint8_t* header;
    u_int16_t header_length;
    u_int64_t timestamp;
    bool is_fin;
    bool is_rst;
};

class FlowBuffer {
private:
    LockFreeChunkPool* pool;
    u_int64_t packet_count;
    u_int64_t last_update_time;
    AggBuffer timestamp_buffer;
    AggBuffer length_buffer;
    AggBuffer header_buffer;
    AggBuffer upstream_data_buffer;
    AggBuffer downstream_data_buffer;
    bool fin_seen;
    bool rst_seen;
    u_int32_t count_after_fin;
public:
    FlowBuffer(LockFreeChunkPool* pool) 
        : pool(pool), 
          timestamp_buffer(pool),
          length_buffer(pool),
          header_buffer(pool), 
          upstream_data_buffer(pool), 
          downstream_data_buffer(pool),
          packet_count(0),
          fin_seen(false),
          rst_seen(false),
          count_after_fin(0) {}
    ~FlowBuffer() = default;

    bool append_packet(struct ParsedPacket* packet){
        if(!this->timestamp_buffer.append(reinterpret_cast<const uint8_t*>(&packet->timestamp), sizeof(packet->timestamp))) return false;

        int32_t packet_length = packet->data_length + packet->header_length;
        packet_length = packet->is_upstream ? packet_length: -packet_length;
        if(!this->length_buffer.append(reinterpret_cast<const uint8_t*>(&packet->data_length), sizeof(packet->data_length))) return false;

        if(!this->header_buffer.append(packet->header, packet->header_length)) return false;

        if(packet->is_upstream){
            if(!this->upstream_data_buffer.append(packet->data, packet->data_length)) return false;
        }
        if(!this->downstream_data_buffer.append(packet->data, packet->data_length)) return false;

        this->packet_count++;
        this->last_update_time = packet->timestamp;
        this->fin_seen = this->fin_seen || packet->is_fin;
        this->rst_seen = this->rst_seen || packet->is_rst;
        if (this->fin_seen) {
            this->count_after_fin++;
        }
        return true;
    }

    bool is_finished() const {
        return (this->fin_seen && this->count_after_fin >= 4) || this-> rst_seen;
    }
    void set_flags(bool _fin_seen, bool _rst_seen, u_int32_t _count_after_fin){
        this->fin_seen = _fin_seen;
        this->rst_seen = _rst_seen;
        this->count_after_fin = _count_after_fin;
    }
    bool get_fin_seen() const {
        return fin_seen;
    }
    bool get_rst_seen() const {
        return rst_seen;
    }
    u_int32_t get_count_after_fin() const {
        return count_after_fin;
    }

    u_int32_t get_packet_count() const {
        return packet_count;
    }
    u_int32_t get_totol_length() const {
        return timestamp_buffer.length() + length_buffer.length() + header_buffer.length() +
               upstream_data_buffer.length() + downstream_data_buffer.length() + sizeof(packet_count);
    }
    u_int32_t get_last_update_time() const {
        return last_update_time;
    }

    u_int32_t get_timestamp_length() const {
        return timestamp_buffer.length();
    }
    u_int32_t get_length_length() const {
        return length_buffer.length();
    }
    u_int32_t get_header_length() const {
        return header_buffer.length();
    }
    u_int32_t get_upstream_data_length() const {
        return upstream_data_buffer.length();
    }
    u_int32_t get_downstream_data_length() const {
        return downstream_data_buffer.length();
    }

    Chunk* get_timestamp_chunk() const {
        return timestamp_buffer.get_head();
    }
    Chunk* get_length_chunk() const {
        return length_buffer.get_head();
    }
    Chunk* get_header_chunk() const {
        return header_buffer.get_head();
    }
    Chunk* get_upstream_data_chunk() const {
        return upstream_data_buffer.get_head();
    }
    Chunk* get_downstream_data_chunk() const {
        return downstream_data_buffer.get_head();
    }
};

#endif