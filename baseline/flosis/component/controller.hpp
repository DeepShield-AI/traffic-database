#ifndef CONTROLLER_HPP_
#define CONTROLLER_HPP_

#include "engine.hpp"
#include "checker.hpp"
#include "dumper.hpp"

struct InitData{
    u_int32_t flow_ring_capacity;
    // u_int32_t storage_ring_capacity;
    // u_int64_t truncate_interval;
    u_int16_t nb_rx;
    // u_int32_t pcap_header_len;
    u_int32_t eth_header_len;
    // size_t hash_num;
    u_int64_t data_disk_size;
    u_int64_t data_block_size;
    // u_int64_t index_disk_size;
    // u_int64_t index_block_size;
    // u_int64_t disk_read_size;

    u_int64_t memory_pool_capacity;
    // u_int64_t memory_pool_list_len_each;
    u_int64_t wbuffer_size_each;

    // u_int64_t data_block_cache_num;
    // u_int64_t index_buffer_cache_num;
    // u_int64_t index_block_cache_num;
    // u_int64_t delay_threshold;
    u_int64_t flow_buffer_len_threshold;
    u_int64_t flow_buffer_time_threshold;

    // u_int64_t bitmap_backup_col_num;

    // u_int64_t file_capacity;
    // u_int32_t index_construct_thread_num;
    // u_int32_t index_persist_thread_num;
    // u_int32_t data_disk_manager_thread_num;
    // u_int32_t index_disk_manager_thread_num;
    // u_int32_t data_memory_manager_thread_num;
    // u_int32_t index_memory_manager_thread_num;
    // u_int32_t data_agent_num_each;
    // u_int32_t index_agent_num_each;
    // u_int32_t agent_ring_depth;
    // u_int32_t agent_ring_idle_time;
    u_int64_t checker_thread_num;

    // u_int32_t direct_storage_thread_num;
    // u_int32_t index_storage_thread_num;
    // u_int32_t max_node;
    // std::string pcap_header;
    // std::string bpf_prog_name;

    std::string data_disk_name;
    u_int64_t data_disk_offset;
    // std::string index_disk_name;
    // u_int64_t index_disk_offset;
    bool bind_core;
    u_int32_t controller_core_id;
    std::vector<u_int32_t> dpdk_core_id_list;
    std::vector<u_int32_t> packet_core_id_list;
    // std::vector<u_int32_t> indexing_core_id_list;
    // std::vector<u_int32_t> persisting_core_id_list;
    std::vector<u_int32_t> data_dumping_core_id_list;
    std::vector<u_int32_t> checking_core_id_list;
    // std::vector<u_int32_t> index_dumping_core_id_list;
    // std::vector<u_int32_t> data_kernel_core_id_list;
    // std::vector<u_int32_t> index_kernel_core_id_list;
    // std::vector<u_int32_t> data_recycling_core_id_list;
    // std::vector<u_int32_t> index_recycling_core_id_list;
};

class Controller{
    u_int64_t data_disk_size;
    u_int64_t data_block_size;

    int data_disk_fd;
    u_int64_t data_disk_offset;

    std::vector<PointerRingBuffer*> wbufferRings;
    std::vector<PointerRingBuffer*> indexRings;
    std::vector<std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash>*> flow_maps;
    
    PointerRingPool* mem_pool;
    std::vector<WBuffer*> wbuffers;
    DPDK* dpdk;

    std::vector<FlowEngine*> engines;
    // std::vector<std::thread*> engineThreads;

    std::vector<FlowChecker*> checkers;
    std::vector<std::thread*> checkerThreads;

    std::vector<Dumper*> dumpers;
    std::vector<std::thread*> dumperThreads;

    void threadsRun();
    void threadsStop();
    void clear();

    void bindCore(u_int32_t cpu);
public:
    Controller(){
        this->wbufferRings = std::vector<PointerRingBuffer*>();
        this->indexRings = std::vector<PointerRingBuffer*>();
        this->mem_pool = nullptr;
        this->wbuffers = std::vector<WBuffer*>();
        this->flow_maps = std::vector<std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash>*>();
        this->dpdk = nullptr;
        this->engines = std::vector<FlowEngine*>();
        this->checkers = std::vector<FlowChecker*>();
        this->checkerThreads = std::vector<std::thread*>();
        this->dumpers = std::vector<Dumper*>();
        this->dumperThreads = std::vector<std::thread*>();
        
    }
    ~Controller(){
        this->clear();
    }
    void init(InitData init_data);
    void run();
};

#endif