#ifndef FLOWBUFFER_HPP_
#define FLOWBUFFER_HPP_
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <cstring>
#include <list>
#include "aggBuffer.hpp"

struct ParsedPacket {
    bool is_upstream;         // 上行方向
    char* data;
    u_int16_t data_length;
    char* header;
    u_int16_t header_length;
    u_int64_t timestamp;
    bool is_fin;
    bool is_rst;
};

class Buffer{
    char* buffer;
    u_int64_t len;
};

class FlowBuffer {
private:
    u_int64_t packet_count;
    u_int64_t last_update_time;
    PointerRingPool* pool;

    AggBuffer timestamp_buffer;
    AggBuffer length_buffer;
    AggBuffer header_buffer;
    AggBuffer upstream_data_buffer;
    AggBuffer downstream_data_buffer;
    bool fin_seen;
    bool rst_seen;
    u_int32_t count_after_fin;
    bool finished;
public:
    FlowBuffer(PointerRingPool* pool) 
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
        if(!this->timestamp_buffer.write((char*)(&packet->timestamp), sizeof(packet->timestamp))) return false;

        int32_t packet_length = packet->data_length + packet->header_length;
        packet_length = packet->is_upstream ? packet_length: -packet_length;
        if(!this->length_buffer.write((char*)(&packet->data_length), sizeof(packet->data_length))) return false;

        if(!this->header_buffer.write(packet->header, packet->header_length)) return false;

        if(packet->is_upstream){
            if(!this->upstream_data_buffer.write(packet->data, packet->data_length)) return false;
        }
        if(!this->downstream_data_buffer.write(packet->data, packet->data_length)) return false;

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
        return (this->fin_seen && this->count_after_fin >= 4) || this-> rst_seen || this->finished;
    }
    bool threshold_finished(u_int64_t flow_buffer_time_threshold, u_int64_t ts) const {
        if (ts >= flow_buffer_time_threshold + this->last_update_time){
            return true;
        }
        // if (this->packet_count >= flow_buffer_len_threshold){
        //     return true;
        // }
        return false;
    }
    void set_finished(){
        this->finished = true;
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
        return timestamp_buffer.getTotalLen() + length_buffer.getTotalLen() + header_buffer.getTotalLen() +
               upstream_data_buffer.getTotalLen() + downstream_data_buffer.getTotalLen() + sizeof(packet_count);
    }
    u_int32_t get_last_update_time() const {
        return last_update_time;
    }

    u_int32_t get_timestamp_length() const {
        return timestamp_buffer.getTotalLen();
    }
    u_int32_t get_length_length() const {
        return length_buffer.getTotalLen();
    }
    u_int32_t get_header_length() const {
        return header_buffer.getTotalLen();
    }
    u_int32_t get_upstream_data_length() const {
        return upstream_data_buffer.getTotalLen();
    }
    u_int32_t get_downstream_data_length() const {
        return downstream_data_buffer.getTotalLen();
    }
    AggBuffer* get_timestamp_buffer() {
        return &timestamp_buffer;
    }
    AggBuffer* get_length_buffer() {
        return &length_buffer;
    }
    AggBuffer* get_header_buffer() {
        return &header_buffer;
    }
    AggBuffer* get_upstream_data_buffer() {
        return &upstream_data_buffer;
    }
    AggBuffer* get_downstream_data_buffer() {
        return &downstream_data_buffer;
    }
};

#endif