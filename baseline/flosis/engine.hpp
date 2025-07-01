#ifndef FLOSIS_ENGINE_HPP_
#define FLOSIS_ENGINE_HPP_
#include <unordered_map>
#include <list>
#include <thread>
#include <atomic>
#include <memory>
#include <vector>
#include <chrono>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_malloc.h>

#include <stdint.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include "lib/flowbuffer.hpp"
#include "lib/wbuffer.hpp"
#include "../../dpdk_lib/packetAggregator.hpp"
#include "../../dpdk_lib/dpdk.hpp"
#include "../../dpdk_lib/header.hpp"
#include "../../dpdk_lib/util.hpp"
#include "../../dpdk_lib/pointerRingBuffer.hpp"

uint64_t swap_endianness(uint64_t value) {
    return ((value >> 56) & 0x00000000000000FFULL) | // byte 0
           ((value >> 40) & 0x000000000000FF00ULL) | // byte 1
           ((value >> 24) & 0x00000000FF000000ULL) | // byte 2
           ((value >> 8)  & 0x00FF000000000000ULL) | // byte 3
           ((value << 8)  & 0xFF00000000000000ULL) | // byte 4
           ((value << 24) & 0x0000FF0000000000ULL) | // byte 5
           ((value << 40) & 0x000000FF00000000ULL) | // byte 6
           ((value << 56) & 0x00000000000000FFULL);   // byte 7
}

class FlowEngine{
private:
    const u_int32_t eth_header_len;
    const u_int32_t flow_buffer_len_threshold;
    const u_int32_t flow_buffer_time_threshold;

    DPDK* dpdk;
    u_int16_t port_id;
    u_int16_t rx_id;

    LockFreeChunkPool* pool;
    std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash> flow_map;
    LockFreeChunkPool* dump_pool;
    WBuffer wbuffer;
    PointerRingBuffer* wbuffer_to_dump_list;

    void readPacket(struct rte_mbuf *buf,u_int64_t ts,ParsedPacket* packet, FlowMetadata* flow_meta);
    FlowBuffer* writePacketToMap(ParsedPacket& packet, FlowMetadata& flow_meta);
    void writeFlowtoWBuffer(FlowBuffer* buffer);

public:
    FlowEngine(const u_int32_t eth_header_len, const u_int32_t flow_buffer_len_threshold, const u_int32_t flow_buffer_time_threshold, DPDK* dpdk, LockFreeChunkPool* pool, LockFreeChunkPool* dump_pool, u_int16_t port_id, u_int16_t rx_id, PointerRingBuffer* wbuffer_to_dump_list)
        : eth_header_len(eth_header_len), flow_buffer_len_threshold(flow_buffer_len_threshold), flow_buffer_time_threshold(flow_buffer_time_threshold) ,dpdk(dpdk), pool(pool), dump_pool(dump_pool), wbuffer(WBuffer(dump_pool)), port_id(port_id), rx_id(rx_id),wbuffer_to_dump_list(wbuffer_to_dump_list) {
        this->flow_map = std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash>();
    }
    ~FlowEngine(){
        for(auto& pair : flow_map) {
            delete pair.second;
        }
        flow_map.clear();
    }
    int run();
    void asynchronousStop();
    static int launch(void *arg){
        return static_cast<FlowEngine*>(arg)->run();
    }
};

#endif