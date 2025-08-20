#ifndef DPDKREADER_HPP_
#define DPDKREADER_HPP_
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <atomic>

// #include "../dpdk_lib/memoryBuffer.hpp"
#include "../dpdk_lib/packetAggregator.hpp"
#include "../dpdk_lib/header.hpp"
// #include "../dpdk_lib/util.hpp"
#include "../dpdk_lib/dpdk.hpp"
// #include "../dpdk_lib/pointerRingBuffer.hpp"
#include "../dpdk_lib/singleRingBuffer.hpp"
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


// #define BURST_SIZE 32


struct PacketMeta{
    array_list_header* header;
    const char* data;
    u_int32_t len;
    // u_int8_t tag_num;
    // Tag* tags;
};

// read packet from pcap, equivalent like extractor, lower substitution of trace catcher
class DPDKReader{
    // const u_int32_t pcap_header_len;
    const u_int32_t eth_header_len;
    const u_int64_t disk_size;
    const u_int64_t block_size;
    const u_int64_t block_num;
    const u_int64_t cell_size;
    const u_int64_t cell_num;

    u_int16_t port_id;
    u_int16_t rx_id;
    DPDK* dpdk;

    PacketAggregator* packetAggregator;

    // write only
    // std::vector<PointerRingBuffer*>* indexRings;
    PointerRingBuffer* indexRing;
    DataBlockBuffer* block_buffer;
    std::atomic_uint_fast64_t* diskWriteCell;
    u_int64_t writeCell;
    u_int64_t writeOffset;
    // MemoryPool* indexMemoryPool;

    std::atomic_bool stop;

    u_int64_t duration_time;
    u_int64_t create_time;
    u_int64_t cal_time;
    u_int64_t allocate_time;
    u_int64_t init_time;
    u_int64_t put_time;
    u_int64_t byteLen;

    bool bind_core;
    u_int32_t core_id;

    u_int64_t thread_id;

    void writeBefore(const char* data, u_int32_t len, u_int64_t last_offset);

    //read packet of offset from file;
    void readPacket(struct rte_mbuf *buf,u_int64_t ts,PacketMeta* meta);
    u_int64_t getOffset(PacketMeta& meta);
    void writePacketToPacketBuffer(PacketMeta& meta, u_int64_t ts, u_int64_t _offset, bool new_index);
    FlowMetadata getFlowMetaData(PacketMeta& meta);

    u_int64_t calValue(u_int64_t _offset);
    u_int64_t calDiff(u_int64_t offset, u_int64_t last_offset);
    u_int64_t calIndexNodeLen(u_int32_t key_len, u_int32_t level);

    bool writeIndexToRing(u_int64_t value, FlowMetadata& meta, u_int64_t ts);
    void bindCore(u_int32_t cpu);

public:
    DPDKReader(u_int32_t eth_header_len, u_int64_t disk_size, u_int64_t block_size, u_int64_t cell_size, DPDK* dpdk, PointerRingBuffer* ring,DataBlockBuffer* block_buffer, std::atomic_uint_fast64_t* diskWritePos, u_int16_t port_id, u_int16_t rx_id, u_int64_t capacity, bool bind_core = false, u_int32_t core_id = 0):
    eth_header_len(eth_header_len),disk_size(disk_size),block_size(block_size),block_num(disk_size/block_size),cell_size(cell_size),cell_num(disk_size/cell_size),dpdk(dpdk),indexRing(ring),block_buffer(block_buffer),diskWriteCell(diskWritePos),port_id(port_id),rx_id(rx_id){
        if(this->cell_num & (this->cell_num - 1)){
            printf("DPDK reader error: cell number %lu is not power of 2!\n",this->cell_num);
            this->packetAggregator = nullptr;
            throw std::runtime_error("Unsupport block size");
        }
        if(this->block_size % this->cell_size){
            printf("DPDK reader error: block size %lu is not multiple of cell size %lu!\n",this->block_size, this->cell_size);
            this->packetAggregator = nullptr;
            throw std::runtime_error("Unsupport block size");
        }

        this->stop = true;
        this->duration_time = 0;
        this->create_time = 0;
        this->cal_time = 0;
        this->allocate_time = 0;
        this->init_time = 0;
        this->put_time = 0;
        this->byteLen = 0;

        this->packetAggregator = new PacketAggregator(capacity, std::numeric_limits<uint64_t>::max());
        this->bind_core = bind_core;
        this->core_id = core_id;
        this->diskWriteCell->store(0);
        this->writeCell = std::numeric_limits<uint64_t>::max();
        this->writeOffset = 0;
        this->thread_id = this->block_buffer->addWriteThread();
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