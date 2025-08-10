#ifndef DPDKREADER_HPP_
#define DPDKREADER_HPP_
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <atomic>

// #include "../dpdk_lib/memoryBuffer.hpp"
#include "../dpdk_lib/header.hpp"
#include "../dpdk_lib/util.hpp"
#include "../dpdk_lib/dpdk.hpp"
#include "../dpdk_lib/pointerRingBuffer.hpp"
#include "../dpdk_lib/packetAggregator.hpp"
#include "../dpdk_lib/diskAgent.hpp"
#include "../dpdk_lib/dataBlockbuffer.hpp"
#include "../dpdk_lib/memoryPool.hpp"
#include "../dpdk_lib/skipList.hpp"

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_malloc.h>

#include <stdint.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <arpa/inet.h>


#define BURST_SIZE 32


struct PacketMeta{
    array_list_header* header;
    const char* data;
    u_int32_t len;
    // u_int8_t tag_num;
    // Tag* tags;
};

// read packet from pcap, equivalent like extractor, lower substitution of trace catcher
class DPDKReader{
    const u_int32_t pcap_header_len;
    const u_int32_t eth_header_len;
    const u_int64_t disk_size;
    const u_int64_t block_size;
    const u_int64_t block_num;

    u_int16_t port_id;
    u_int16_t rx_id;
    DPDK* dpdk;

    PacketAggregator* packetAggregator;

    // write only
    std::vector<PointerRingBuffer*>* indexRings;
    DataBlockBuffer* block_buffer;
    std::atomic_uint_fast64_t* diskWritePos;
    MemoryPool* indexMemoryPool;

    std::atomic_bool stop;

    u_int64_t duration_time;
    u_int64_t byteLen;

    bool bind_core;
    u_int32_t core_id;

    u_int64_t thread_id;

    void writeBefore(const char* data, u_int32_t len, u_int64_t last_offset);

    //read packet of offset from file;
    void readPacket(struct rte_mbuf *buf,u_int64_t ts,PacketMeta* meta);
    u_int64_t writePacketToPacketBuffer(PacketMeta& meta, u_int64_t ts);
    FlowMetadata getFlowMetaData(PacketMeta& meta);

    u_int64_t calValue(u_int64_t _offset);
    u_int64_t calDiff(u_int64_t offset, u_int64_t last_offset);
    u_int64_t calIndexNodeLen(u_int32_t key_len, u_int32_t level);

    bool writeIndexToRing(u_int64_t value, FlowMetadata meta, u_int64_t ts);
    void bindCore(u_int32_t cpu);

public:
    DPDKReader(u_int32_t pcap_header_len, u_int32_t eth_header_len, u_int64_t disk_size, u_int64_t block_size, DPDK* dpdk, std::vector<PointerRingBuffer*>* rings,DataBlockBuffer* block_buffer, std::atomic_uint_fast64_t* diskWritePos, MemoryPool* indexMemoryPool, u_int16_t port_id, u_int16_t rx_id, u_int64_t capacity, bool bind_core, u_int32_t core_id):
    pcap_header_len(pcap_header_len),eth_header_len(eth_header_len),disk_size(disk_size),block_size(block_size),block_num(disk_size/block_size),dpdk(dpdk),indexRings(rings),block_buffer(block_buffer),diskWritePos(diskWritePos),indexMemoryPool(indexMemoryPool),port_id(port_id),rx_id(rx_id){
        if(this->block_num & (this->block_size - 1)){
            printf("DPDK reader error: block size %u is not power of 2!\n",this->block_size);
            this->packetAggregator = nullptr;
            return;
        }

        this->stop = true;
        this->duration_time = 0;
        this->byteLen = 0;

        this->packetAggregator = new PacketAggregator(capacity, std::numeric_limits<uint64_t>::max());
        this->bind_core = bind_core;
        this->core_id = core_id;
        this->diskWritePos->store(0);
        this->thread_id = std::numeric_limits<uint64_t>::max();
    }
    ~DPDKReader(){
        if (this->packetAggregator != nullptr) delete this->packetAggregator;
    }
    void setThreadID(u_int64_t threadID);
    int run();
    void asynchronousStop();
    static int launch(void *arg){
        return static_cast<DPDKReader*>(arg)->run();
    }
};

#endif