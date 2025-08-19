#ifndef CONTROLLER_HPP_
#define CONTROLLER_HPP_
#include <iostream>
#include <thread>
#include <vector>
#include <numa.h>
#include <numaif.h>
#include "dpdkReader.hpp"
#include "indexGenerator.hpp"
#include "indexPersister.hpp"
#include "diskManager.hpp"
#include "memoryManager.hpp"

// #include "storage.hpp"
// #include "truncateChecker.hpp"
#include "querier.hpp"
// #include "directStorage.hpp"
// #include "indexStorage.hpp"

struct InitData{
    u_int32_t index_ring_capacity;
    // u_int32_t storage_ring_capacity;
    // u_int64_t truncate_interval;
    u_int16_t nb_rx;
    // u_int32_t pcap_header_len;
    u_int32_t eth_header_len;
    size_t hash_num;
    u_int64_t data_disk_size;
    u_int64_t data_block_size;
    u_int64_t index_disk_size;
    u_int64_t index_block_size;

    u_int64_t memory_pool_capacity_each;
    u_int64_t memory_pool_list_len_each;

    u_int64_t data_block_cache_num;
    u_int64_t index_buffer_cache_num;
    u_int64_t index_block_cache_num;
    u_int64_t delay_threshold;

    u_int64_t bitmap_backup_col_num;

    // u_int64_t file_capacity;
    u_int32_t index_construct_thread_num;
    u_int32_t index_persist_thread_num;
    u_int32_t data_disk_manager_thread_num;
    u_int32_t index_disk_manager_thread_num;
    u_int32_t data_memory_manager_thread_num;
    u_int32_t index_memory_manager_thread_num;
    u_int32_t data_agent_num_each;
    u_int32_t index_agent_num_each;
    u_int32_t agent_ring_depth;
    u_int32_t agent_ring_idle_time;

    // u_int32_t direct_storage_thread_num;
    // u_int32_t index_storage_thread_num;
    // u_int32_t max_node;
    // std::string pcap_header;
    // std::string bpf_prog_name;

    std::string data_disk_name;
    u_int64_t data_disk_offset;
    std::string index_disk_name;
    u_int64_t index_disk_offset;
    bool bind_core;
    u_int32_t controller_core_id;
    std::vector<u_int32_t> dpdk_core_id_list;
    std::vector<u_int32_t> packet_core_id_list;
    std::vector<u_int32_t> indexing_core_id_list;
    std::vector<u_int32_t> persisting_core_id_list;
    std::vector<u_int32_t> data_dumping_core_id_list;
    std::vector<u_int32_t> index_dumping_core_id_list;
    std::vector<u_int32_t> data_kernel_core_id_list;
    std::vector<u_int32_t> index_kernel_core_id_list;
    std::vector<u_int32_t> data_recycling_core_id_list;
    std::vector<u_int32_t> index_recycling_core_id_list;
};

class Controller{
private:
    // std::string pcapHeader;

    u_int64_t data_disk_size;
    u_int64_t data_block_size;
    u_int64_t index_disk_size;
    u_int16_t index_block_size;

    int data_disk_fd;
    u_int64_t data_disk_offset;
    int index_disk_fd;
    u_int64_t index_disk_offset;

    std::vector<u_int32_t> data_kernel_core_id_list;
    std::vector<u_int32_t> index_kernel_core_id_list;

    std::vector<PointerRingBuffer*>* indexRings;
    std::atomic_uint64_t* dataWritePos;
    // std::vector<MemoryPool*>* indexMemoryPools;

    IndexBuffer* indexBuffer;
    IndexBlockBuffer* indexBlockBuffer;
    // DiskBuffer* indexDiskBuffer;
    std::atomic_uint64_t* indexWritePos;

    DataBlockBuffer* dataBlockBuffer;
    DiskBuffer* diskMeta;

    DPDK* dpdk;
    std::vector<DiskAgent*> dataAgents;
    std::vector<DiskAgent*> indexAgents;
    BitMap* bitmap;
    
    std::vector<DPDKReader*> readers;

    std::vector<IndexGenerator*> indexGenerators;
    std::vector<std::thread*> indexGeneratorThreads;

    std::vector<IndexPersister*> indexPersisters;
    std::vector<std::thread*> indexPersisterThreads;

    std::vector<DiskManager*> dataDiskManagers;
    std::vector<std::thread*> dataDiskManagerThreads;

    std::vector<DiskManager*> indexDiskManagers;
    std::vector<std::thread*> indexDiskManagerThreads;

    std::vector<MemoryManager*> dataMemoryManagers;
    std::vector<std::thread*> dataMemoryManagerThreads;

    std::vector<MemoryManager*> indexMemoryManagers;
    std::vector<std::thread*> indexMemoryManagerThreads;

    // std::vector<IndexStorage*> indexStorages;
    // std::vector<std::thread*> indexStorageThreads;
    // Storage* storage;
    // std::thread* storageThread;
    // TruncateChecker* checker;
    // std::thread* checkThread;
    // std::vector<DirectStorage*> directStorages;
    // std::vector<std::thread*> directStorageThreads;

    Querier* querier;
    std::thread* querierThread;
    DiskAgent* querierIndexAgent;
    DiskAgent* querierDataAgent;

    void threadsRun();
    void queryThreadRun();
    void threadsStop();
    void clear();

    void bindCore(u_int32_t cpu);

public:
    Controller(){
        this->indexRings = new std::vector<PointerRingBuffer*>();
        this->dataWritePos = new std::atomic_uint64_t(0);
        // this->indexMemoryPools = new std::vector<MemoryPool*>();
    
        this->indexBuffer = nullptr;
        this->indexBlockBuffer = nullptr;
        // this->indexDiskBuffer = nullptr;
        this->indexWritePos = new std::atomic_uint64_t(0);

        this->dataBlockBuffer = nullptr;
        this->diskMeta = nullptr;

        // this->indexBuffers = new std::vector<IndexBuffer*>();
        // this->truncators = new std::vector<Truncator*>(INDEX_NUM,nullptr);
        // this->storageRing = nullptr;
        // this->storageMetas = nullptr;
        this->dpdk = nullptr;
        this->dataAgents = std::vector<DiskAgent*>();
        this->indexAgents = std::vector<DiskAgent*>();
        this->bitmap = nullptr;

        // this->buffers = std::vector<MemoryBuffer*>();
        this->readers = std::vector<DPDKReader*>();
        this->indexGenerators = std::vector<IndexGenerator*>();
        this->indexGeneratorThreads = std::vector<std::thread*>();
        this->indexPersisters = std::vector<IndexPersister*>();
        this->indexPersisterThreads = std::vector<std::thread*>();
        this->dataDiskManagers = std::vector<DiskManager*>();
        this->dataDiskManagerThreads = std::vector<std::thread*>();
        this->indexDiskManagers = std::vector<DiskManager*>();
        this->indexDiskManagerThreads = std::vector<std::thread*>();
        this->dataMemoryManagers = std::vector<MemoryManager*>();
        this->dataMemoryManagerThreads = std::vector<std::thread*>();
        this->indexMemoryManagers = std::vector<MemoryManager*>();
        this->indexMemoryManagerThreads = std::vector<std::thread*>();
        // this->storage = nullptr;
        // this->checker = nullptr;
        // this->storageThread = nullptr;
        // this->checkThread = nullptr;
        this->querier = nullptr;
        this->querierThread = nullptr;
        // this->directStorages = std::vector<DirectStorage*>();
        // this->directStorageThreads = std::vector<std::thread*>();
        // this->indexStorages = std::vector<IndexStorage*>();
        // this->indexStorageThreads = std::vector<std::thread*>();
    }
    ~Controller(){
        this->clear();
    }
    void init(InitData init_data);
    void run();
};

#endif