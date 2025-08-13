#include "controller.hpp"

void Controller::threadsRun(){
    // this->storageThread = new std::thread(&Storage::run,this->storage);

    // this->checkThread = new std::thread(&TruncateChecker::run,this->checker);
    // std::thread t = std::thread(&DPDKReader::run,readers[0]);

    // sleep(3);

    // for(auto s:this->indexStorages){
    //     std::thread* indexStorageThread = new std::thread(&IndexStorage::run, s);
    //     this->indexStorageThreads.push_back(indexStorageThread);
    // }

    // for(auto s:this->directStorages){
    //     std::thread* directStorageThread = new std::thread(&DirectStorage::run, s);
    //     this->directStorageThreads.push_back(directStorageThread);
    // }

    u_int32_t index = 0;
    for(auto da: this->dataAgents){
        da->kernel_run(this->data_kernel_core_id_list[index]);
        index ++;
    }
    index = 0;
    for(auto ia: this->indexAgents){
        ia->kernel_run(this->index_kernel_core_id_list[index]);
        index ++;
    }

    for(auto dm: this->dataMemoryManagers){
        std::thread* t = new std::thread(&MemoryManager::run,dm);
        this->dataMemoryManagerThreads.push_back(t);
    }
    for(auto im: this->indexMemoryManagers){
        std::thread* t = new std::thread(&MemoryManager::run,im);
        this->indexMemoryManagerThreads.push_back(t);
    }

    for(auto dd: this->dataDiskManagers){
        std::thread* t = new std::thread(&DiskManager::run,dd);
        this->dataDiskManagerThreads.push_back(t);
    }
    for(auto id: this->indexDiskManagers){
        std::thread* t = new std::thread(&DiskManager::run,id);
        this->indexDiskManagerThreads.push_back(t);
    }

    for(auto ip: this->indexPersisters){
        std::thread* t = new std::thread(&IndexPersister::run, ip);
        this->indexPersisterThreads.push_back(t);
    }

    for(auto ig:this->indexGenerators){
        std::thread* t = new std::thread(&IndexGenerator::run,ig);
        this->indexGeneratorThreads.push_back(t);
    }

    unsigned lcore_id;
    u_int16_t queue_id = 0;
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        printf("Worker core ID: %u\n", queue_id);
        int ret = rte_eal_remote_launch(&DPDKReader::launch, readers[queue_id], lcore_id);
        queue_id ++;
        if (ret < 0){
            rte_exit(EXIT_FAILURE, "Error launching lcore %u\n", lcore_id);
        }
    }
    
    // readers[0]->asynchronousStop();
    // t.join();
}

// void Controller::queryThreadRun(){
//     this->querierThread = new std::thread(&Querier::run,this->querier);
// }

void Controller::threadsStop(){
    for(auto r:this->readers){
        r->asynchronousStop();
    }
    unsigned lcore_id;
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        rte_eal_wait_lcore(lcore_id);
        printf("lcore %u stop.\n",lcore_id);
    }

    for(auto ir:*this->indexRings){
        ir->asynchronousStop();
    }
    for(u_int32_t i=0;i<this->indexGenerators.size();++i){
        this->indexGenerators[i]->asynchronousStop();
        this->indexGeneratorThreads[i]->join();
    }

    for(u_int32_t i=0;i<this->dataDiskManagers.size();++i){
        this->dataDiskManagers[i]->asynchronousStop();
        this->dataDiskManagerThreads[i]->join();
    }

    for(u_int32_t i=0;i<this->indexPersisters.size();++i){
        this->indexPersisters[i]->asynchronousStop();
        this->indexPersisterThreads[i]->join();
    }

    for(u_int64_t i=0;i<this->indexDiskManagers.size();++i){
        this->indexDiskManagers[i]->asynchronousStop();
        this->indexDiskManagerThreads[i]->join();
    }

    for(u_int64_t i=0;i<this->dataMemoryManagers.size();++i){
        this->dataMemoryManagers[i]->asynchronousStop();
        this->dataMemoryManagerThreads[i]->join();
    }
    for(u_int64_t i=0;i<this->indexMemoryManagers.size();++i){
        this->indexMemoryManagers[i]->asynchronousStop();
        this->indexMemoryManagerThreads[i]->join();
    }

    // for(u_int32_t i=0;i<this->directStorages.size();++i){
    //     this->directStorages[i]->asynchronousStop();
    //     this->directStorageThreads[i]->join();
    // }

    // for(u_int32_t i=0;i<this->indexStorages.size();++i){
    //     this->indexStorages[i]->asynchronousStop();
    //     this->indexStorageThreads[i]->join();
    // }
    

    // for(int i=0;i<INDEX_NUM;++i){
    //     (*(this->indexRings))[i]->asynchronousStop();
    // }

    // for(int i=0;i<INDEX_NUM;++i){
    //     printf("indexGenerator %d should stop.\n",i);
    //     (*(this->indexRings))[i]->asynchronousStop();
    
    // }

    // this->checker->asynchronousStop();
    // this->checkThread->join();

    // this->storageRing->asynchronousStop();
    // this->storage->asynchronousStop();
    // this->storageThread->join();
}

void Controller::clear(){

    if(this->indexMemoryManagers.size()!=0){
        for(u_int32_t i=0;i<this->indexMemoryManagers.size();++i){
            delete this->indexMemoryManagers[i];
            delete this->indexMemoryManagerThreads[i];
        }
        this->indexMemoryManagers.clear();
        this->indexMemoryManagerThreads.clear();
    }

    if(this->dataMemoryManagers.size()!=0){
        for(u_int32_t i=0;i<this->dataMemoryManagers.size();++i){
            delete this->dataMemoryManagers[i];
            delete this->dataMemoryManagerThreads[i];
        }
        this->dataMemoryManagers.clear();
        this->dataMemoryManagerThreads.clear();
    }

    if(this->indexDiskManagers.size()!=0){
        for(u_int32_t i=0;i<this->indexDiskManagers.size();++i){
            delete this->indexDiskManagers[i];
            delete this->indexDiskManagerThreads[i];
        }
        this->indexDiskManagers.clear();
        this->indexDiskManagerThreads.clear();
    }

    if(this->dataDiskManagers.size()!=0){
        for(u_int32_t i=0;i<this->dataDiskManagers.size();++i){
            delete this->dataDiskManagers[i];
            delete this->dataDiskManagerThreads[i];
        }
        this->dataDiskManagers.clear();
        this->dataDiskManagerThreads.clear();
    }

    if(this->indexPersisters.size()!=0){
        for(u_int32_t i=0;i<this->indexPersisters.size();++i){
            delete this->indexPersisters[i];
            delete this->indexPersisterThreads[i];
        }
        this->indexPersisters.clear();
        this->indexPersisterThreads.clear();
    }

    if(this->indexGenerators.size()!=0){
        for(u_int32_t i=0;i<this->indexGenerators.size();++i){
            delete this->indexGenerators[i];
            delete this->indexGeneratorThreads[i];
        }
        this->indexGenerators.clear();
        this->indexGeneratorThreads.clear();
    }

    if(this->readers.size()!=0){
        for(u_int16_t i = 0;i<this->readers.size();++i){
            delete readers[i];
        }
        readers.clear();
    }
    //         this->indexGenerators[i].clear();
    //         this->indexGeneratorThreads[i].clear();
    //     }
    //     this->indexGenerators.clear();
    //     this->indexGeneratorThreads.clear();
    // }
    // for(u_int32_t i=0;i<this->directStorageThreads.size();++i){
    //     if(this->directStorageThreads[i]!=nullptr){
    //         delete this->directStorageThreads[i];
    //         this->directStorageThreads[i] = nullptr;
    //     }
    // }
    // this->directStorageThreads.clear();

    // for(u_int32_t i=0;i<this->indexStorageThreads.size();++i){
    //     if(this->indexStorageThreads[i]!=nullptr){
    //         delete this->indexStorageThreads[i];
    //         this->indexStorageThreads[i] = nullptr;
    //     }
    // }
    // this->indexStorageThreads.clear();
    

    // for(u_int32_t i=0;i<this->indexStorages.size();++i){
    //     if(this->indexStorages[i]!=nullptr){
    //         delete this->indexStorages[i];
    //         this->indexStorages[i] = nullptr;
    //     }
    // }
    // this->indexStorages.clear();

    // for(u_int32_t i=0;i<this->directStorages.size();++i){
    //     if(this->directStorages[i]!=nullptr){
    //         delete this->directStorages[i];
    //         this->directStorages[i] = nullptr;
    //     }
    // }
    // this->directStorages.clear();
    
    // for(u_int32_t i = 0;i<this->buffers.size();++i){
    //     delete buffers[i];
    // }
    // this->buffers.clear();

    for(u_int32_t i = 0; i<this->indexAgents.size();++i){
        delete this->indexAgents[i];
    }
    this->indexAgents.clear();

    for(u_int32_t i = 0; i<this->dataAgents.size();++i){
        delete this->dataAgents[i];
    }
    this->dataAgents.clear();

    if(this->indexBlockBuffer != nullptr){
        delete this->indexBlockBuffer;
        this->indexBlockBuffer = nullptr;
    }
    if(this->dataBlockBuffer != nullptr){
        delete this->dataBlockBuffer;
        this->dataBlockBuffer = nullptr;
    }
    if(this->indexBuffer != nullptr){
        delete this->indexBuffer;
        this->indexBuffer = nullptr;
    }
    // Todo: first stage index persistance
    if(this->diskMeta != nullptr){
        delete this->diskMeta;
        this->diskMeta = nullptr;
    }
    if(this->bitmap != nullptr){
        delete this->bitmap;
        this->bitmap = nullptr;
    }

    if(this->indexRings!=nullptr){
        for(auto ib:(*(this->indexRings))){
            if(ib!=nullptr){
                delete ib;
                ib = nullptr;
            }
        }
        delete this->indexRings;
        this->indexRings = nullptr;
    }

    if(this->indexMemoryPools!=nullptr){
        for(auto imp:(*(this->indexMemoryPools))){
            if(imp!=nullptr){
                delete imp;
                imp = nullptr;
            }
        }
        delete this->indexMemoryPools;
        this->indexMemoryPools = nullptr;
    }

    if(this->dpdk!=nullptr){
        delete this->dpdk;
        this->dpdk = nullptr;
    }

    delete this->dataWritePos;
    this->dataWritePos = nullptr;
    delete this->indexWritePos;
    this->indexWritePos = nullptr;

    // if(this->storageMetas!=nullptr){
    //     delete[] this->storageMetas;
    //     this->storageMetas = nullptr;
    // }
    // if(this->storageRing!=nullptr){
    //     delete this->storageRing;
    //     this->storageRing = nullptr;
    // }
    // if(this->truncators!=nullptr){
    //     for(int i=0;i<INDEX_NUM;++i){
    //         delete (*(this->truncators))[i];
    //     }
    //     this->truncators->clear();
    //     delete this->truncators;
    // }
    
    // if(this->indexBuffers != nullptr){
    //     for(auto ib:(*(this->indexBuffers))){
    //         if(ib!=nullptr){
    //             delete ib;
    //             ib = nullptr;
    //         }
    //     }
    //     delete this->indexBuffers;
    //     this->indexBuffers = nullptr;
    // }
    // if(this->querier!=nullptr){
    //     delete this->querier;
    // }
    // if(this->querierThread!=nullptr){
    //     delete this->querierThread;
    // }
}

void Controller::init(InitData init_data){
    if(init_data.bind_core){
        this->bindCore(init_data.controller_core_id);
    }

    this->data_disk_fd = open(init_data.data_disk_name.c_str(), O_DIRECT | O_RDWR);
    this->data_disk_offset = init_data.data_disk_offset;
    if (this->data_disk_fd < 0){
        printf("Controller error: open data disk %s failed.\n", init_data.data_disk_name.c_str());
        throw std::runtime_error("open data disk failed");
    }

    if(init_data.index_disk_name == init_data.data_disk_name){
        this->index_disk_fd = this->data_disk_fd;
        this->index_disk_offset = init_data.data_disk_offset;
        if ((init_data.index_disk_offset < init_data.data_disk_offset && this->index_disk_offset + init_data.index_disk_size > init_data.data_disk_offset) ||
            (init_data.index_disk_offset > init_data.data_disk_offset && this->data_disk_offset + init_data.data_disk_size > init_data.index_disk_offset) ||
            (init_data.index_disk_offset == init_data.data_disk_offset)){
            printf("Controller error: index disk %lu should be not in the same range as data disk %lu.\n", init_data.index_disk_offset, init_data.data_disk_offset);
            throw std::runtime_error("index disk is not in the same range as data disk");
        }
    } else {
        this->index_disk_fd = open(init_data.index_disk_name.c_str(), O_DIRECT | O_RDWR);
        this->index_disk_offset = init_data.index_disk_offset;
        if (this->index_disk_fd < 0){
            printf("Controller error: open index disk %s failed.\n", init_data.index_disk_name.c_str());
            throw std::runtime_error("open index disk failed");
        }
    }

    if(init_data.data_disk_size % init_data.data_block_size != 0){
        printf("Controller error: data disk size %lu is not multiple of data block size %lu.\n",init_data.data_disk_size,init_data.data_block_size);
        throw std::runtime_error("data disk size is not multiple of data block size");
    }
    if(init_data.index_disk_size % init_data.index_block_size != 0){
        printf("Controller error: index disk size %lu is not multiple of index block size %lu.\n",init_data.index_disk_size,init_data.index_block_size);
        throw std::runtime_error("index disk size is not multiple of index block size");
    }

    this->data_disk_size = init_data.data_disk_size;
    this->data_block_size = init_data.data_block_size;
    
    this->index_disk_size = init_data.index_disk_size;
    this->index_block_size = init_data.index_block_size;

    this->data_kernel_core_id_list = init_data.data_kernel_core_id_list;
    this->index_kernel_core_id_list = init_data.index_kernel_core_id_list;

    // this->bindCore(24);

    // for(u_int32_t i=0;i<flowMetaEleLens.size();++i){
    //     PointerRingBuffer* ir =  new PointerRingBuffer(init_data.index_ring_capacity);
    //     this->indexRings->push_back(ir);
    // }

    PointerRingBuffer* ir =  new PointerRingBuffer(init_data.index_ring_capacity);
    this->indexRings->push_back(ir);

    for (u_int32_t i=0; i<init_data.nb_rx; ++i){
        MemoryPool* mp = new MemoryPool(init_data.memory_pool_capacity_each, init_data.memory_pool_list_len_each);
        this->indexMemoryPools->push_back(mp);
    }

    this->bitmap = new BitMap((PORT_BIT_LEN + IPV4_BIT_LEN + IPV6_BIT_LEN) * 2, init_data.data_disk_size / init_data.data_block_size, init_data.bitmap_backup_col_num);

    this->indexBuffer = new IndexBuffer(init_data.index_buffer_cache_num, init_data.data_block_size / init_data.data_block_size, this->bitmap, init_data.nb_rx);
    this->indexBlockBuffer = new IndexBlockBuffer(init_data.index_block_cache_num, init_data.index_block_size, init_data.index_disk_size / init_data.index_block_size, init_data.index_persist_thread_num);

    this->dataBlockBuffer = new DataBlockBuffer(init_data.data_block_cache_num, init_data.data_block_size, init_data.data_disk_size / init_data.data_block_size, init_data.nb_rx, init_data.delay_threshold);
    this->diskMeta = new DiskBuffer(init_data.data_disk_size / init_data.data_block_size, this->bitmap, init_data.hash_num);

    this->dpdk = new DPDK(init_data.nb_rx,1,init_data.bind_core,init_data.dpdk_core_id_list);

    for (u_int32_t i = 0; i <init_data.data_agent_num_each * init_data.data_disk_manager_thread_num; ++i){
        DiskAgent* da = new DiskAgent(init_data.data_disk_size, init_data.data_block_size, init_data.data_disk_offset, this->data_disk_fd, init_data.agent_ring_depth, init_data.agent_ring_idle_time);
        this->dataAgents.push_back(da);
    }
    for (u_int32_t i = 0; i <init_data.index_agent_num_each * init_data.index_disk_manager_thread_num; ++i){
        DiskAgent* ia = new DiskAgent(init_data.index_disk_size, init_data.index_block_size, init_data.index_disk_offset, this->index_disk_fd, init_data.agent_ring_depth, init_data.agent_ring_idle_time);
        this->indexAgents.push_back(ia);
    }
    
    // for(u_int32_t i=0;i<init_data.direct_storage_thread_num;++i){
    //     DirectStorage* directStorage = new DirectStorage(i);
    //     this->directStorages.push_back(directStorage);
    // }

    // for(u_int32_t i=0;i<init_data.index_storage_thread_num;++i){
    //     // IndexStorage* indexStorage = new IndexStorage((*(this->indexBuffers))[i%flowMetaEleLens.size()],i%flowMetaEleLens.size());
    //     IndexStorage* indexStorage = new IndexStorage();
    //     this->indexStorages.push_back(indexStorage);
    // }

    // std::vector<SkipListMeta> metas = std::vector<SkipListMeta>();
    // for(auto len:flowMetaEleLens){
    //     SkipListMeta meta = {
    //         .keyLen = len,
    //         .valueLen = sizeof(u_int64_t),
    //         .maxLvl = len*8,
    //     };
    //     metas.push_back(meta);
    // }

    // SkipListMeta tagMeta = {
    //     .keyLen = sizeof(u_int64_t),
    //     .valueLen = sizeof(u_int64_t),
    //     .maxLvl = sizeof(u_int64_t)*8,
    // };

    // for(u_int32_t i=0;i<flowMetaEleLens.size();++i){
    //     IndexBuffer* ib = new IndexBuffer(5,metas[i],init_data.max_node);
    //     this->indexStorages[i%init_data.index_storage_thread_num]->addBuffer(ib,i);
    //     (*(this->indexBuffers)).push_back(ib);
    // }

    // for(u_int32_t i=0;i<MAX_TAG_TYPE;++i){
    //     IndexBuffer* ib = new IndexBuffer(5,tagMeta,init_data.max_node);
    //     this->indexStorages[(i + flowMetaEleLens.size()) %init_data.index_storage_thread_num]->addBuffer(ib,i + flowMetaEleLens.size());
    //     (*(this->indexBuffers)).push_back(ib);
    // }

    for(u_int32_t i=0; i<init_data.data_memory_manager_thread_num; ++i){
        MemoryManager* dm = new MemoryManager(init_data.data_block_size, this->dataBlockBuffer, AgentType::DATA_AGENT);
        if (init_data.bind_core){
            dm->setBindCore(init_data.data_recycling_core_id_list[i]);
        }
        this->dataMemoryManagers.push_back(dm);
    }
    for(u_int32_t i=0; i<init_data.index_memory_manager_thread_num; ++i){
        MemoryManager* im = new MemoryManager(init_data.index_block_size, this->indexBlockBuffer, AgentType::INDEX_AGENT);
        if (init_data.bind_core){
            im->setBindCore(init_data.index_recycling_core_id_list[i]);
        }
        this->indexMemoryManagers.push_back(im);
    }

    for(u_int32_t i=0; i<init_data.data_disk_manager_thread_num; ++i){
        DiskManager* dd = new DiskManager(init_data.data_disk_size, init_data.data_block_size, this->dataBlockBuffer, AgentType::DATA_AGENT, this->diskMeta);
        if (init_data.bind_core){
            dd->setBindCore(init_data.data_dumping_core_id_list[i]);
        }
        this->dataDiskManagers.push_back(dd);
    }
    for(u_int32_t i=0; i<init_data.index_disk_manager_thread_num; ++i){
        DiskManager* id = new DiskManager(init_data.index_disk_size, init_data.index_block_size, this->indexBlockBuffer, AgentType::INDEX_AGENT,nullptr);
        if (init_data.bind_core){
            id->setBindCore(init_data.index_dumping_core_id_list[i]);
        }
        this->indexDiskManagers.push_back(id);
    }

    for(u_int32_t i=0; i<init_data.index_persist_thread_num; ++i){
        IndexPersister* ip;
        if (init_data.bind_core){
            ip = new IndexPersister(init_data.data_disk_size, init_data.data_block_size, this->indexBuffer, this->indexBlockBuffer, this->diskMeta, this->indexMemoryPools, this->indexWritePos, init_data.bind_core, init_data.persisting_core_id_list[i]);
        }else{
            ip = new IndexPersister(init_data.data_disk_size, init_data.data_block_size, this->indexBuffer, this->indexBlockBuffer, this->diskMeta, this->indexMemoryPools, this->indexWritePos);
        }
        this->indexPersisters.push_back(ip);
    }

    for(u_int32_t i=0;i<init_data.index_construct_thread_num;++i){
        // IndexGenerator* ig = new IndexGenerator((*(this->indexRings))[0],this->indexBuffers,(*(this->indexBuffers))[0]->getCacheCount(),i*2+42);
        IndexGenerator* ig;
        if (init_data.bind_core){
            // ig = new IndexGenerator((*(this->indexRings))[0],this->indexBuffers,(*(this->indexBuffers))[0]->getCacheCount(),i,init_data.bind_core,init_data.indexing_core_id_list[i]);
            ig = new IndexGenerator((*(this->indexRings))[0],this->indexBuffer, i, init_data.bind_core, init_data.indexing_core_id_list[i]);
        }else{
            // ig = new IndexGenerator((*(this->indexRings))[0],this->indexBuffers,(*(this->indexBuffers))[0]->getCacheCount(),i);
            ig = new IndexGenerator((*(this->indexRings))[0],this->indexBuffer, i);
        }
        this->indexGenerators.push_back(ig);
    }

    for(u_int16_t i = 0;i<init_data.nb_rx;++i){
        // std::string file_name = "./data/input/" + std::to_string(0) + "-" + std::to_string(i) + ".pcap";
        // MemoryBuffer* buffer = new MemoryBuffer(0,init_data.file_capacity,5,file_name);
        // this->buffers.push_back(buffer);
        // this->directStorages[i%init_data.direct_storage_thread_num]->addBuffer(buffer);
        DPDKReader* reader;
        if (init_data.bind_core){
            // reader = new DPDKReader(init_data.pcap_header_len,init_data.eth_header_len,dpdk,this->indexRings,0,i,init_data.file_capacity,buffer, init_data.bind_core, init_data.packet_core_id_list[i]);
            reader = new DPDKReader(init_data.eth_header_len, init_data.data_disk_size, init_data.data_block_size, this->dpdk, this->indexRings, this->dataBlockBuffer, this->dataWritePos, (*(this->indexMemoryPools))[i], 0, i, init_data.delay_threshold * init_data.data_block_size, init_data.bind_core, init_data.packet_core_id_list[i]);
        }else{
            reader = new DPDKReader(init_data.eth_header_len, init_data.data_disk_size, init_data.data_block_size, this->dpdk, this->indexRings, this->dataBlockBuffer, this->dataWritePos, (*(this->indexMemoryPools))[i], 0, i, init_data.delay_threshold * init_data.data_block_size);
        }
        this->readers.push_back(reader);
    }

    printf("Detected logical cores: %u\n", rte_lcore_count());

    // this->bpf_prog_name = init_data.bpf_prog_name;

    // this->storage = new Storage(this->storageRing,this->storageMetas);

    // this->checker = new TruncateChecker(this->truncators);

    // this->pcapHeader = init_data.pcap_header;

    // this->querier = new Querier(this->storageMetas,init_data.pcap_header);
}

void Controller::bindCore(u_int32_t cpu){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    pthread_t thread = pthread_self();

    int set_result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (set_result != 0) {
        std::cerr << "Error setting thread affinity: " << set_result << std::endl;
    }

    // 确认设置是否成功
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    if (CPU_ISSET(cpu, &cpuset)) {
        printf("Controller log: %lu bind to cpu %d.\n",thread,cpu);
    } else {
        printf("Controller warning: %lu failed to bind to cpu %d!\n",thread,cpu);
    }
}

void Controller::run(){

    std::cout << "Controller log: run." << std::endl;
    this->threadsRun();

    // this->queryThreadRun();

    printf("wait.\n");
    
    // for(u_int16_t i=0; i<this->readers.size(); ++i){
    //     if(this->dpdk->loadBPF(0, i, this->bpf_prog_name)){
    //         printf("Controller error: load bpf fail at %u\n",i);
    //     }
    // }
    // std::this_thread::sleep_for(std::chrono::seconds(4));
    // for(u_int16_t i=0; i<this->readers.size(); ++i){
    //     this->dpdk->unloadBPF(0, i);
    // }
    // std::this_thread::sleep_for(std::chrono::seconds(1));

    char stop;

    std::cin>>stop;
    
    // this->querierThread->join();
    this->threadsStop();
}