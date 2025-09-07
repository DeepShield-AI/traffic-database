#include "controller.hpp"

void Controller::threadsRun(){
    for(auto dp: this->dumpers){
        std::thread* t = new std::thread(&Dumper::run,dp);
        this->dumperThreads.push_back(t);
    }
    // for(auto fc: this->checkers){
    //     std::thread* t = new std::thread(&FlowChecker::run,fc);
    //     this->checkerThreads.push_back(t);
    // }
    unsigned lcore_id;
    u_int16_t queue_id = 0;
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        printf("Worker core ID: %u\n", queue_id);
        int ret = rte_eal_remote_launch(&FlowEngine::launch, this->engines[queue_id], lcore_id);
        queue_id ++;
        if (ret < 0){
            rte_exit(EXIT_FAILURE, "Error launching lcore %u\n", lcore_id);
        }
    }
}
void Controller::threadsStop(){
    for(auto e:this->engines){
        e->asynchronousStop();
    }
    unsigned lcore_id;
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        rte_eal_wait_lcore(lcore_id);
        printf("lcore %u stop.\n",lcore_id);
    }

    for(auto ir: this->indexRings){
        ir->asynchronousStop();
    }

    // printf("a\n");
    
    // for(u_int32_t i=0;i<this->checkers.size();++i){
    //     this->checkers[i]->asynchronousStop();
    //     this->checkerThreads[i]->join();
    // }

    // printf("b\n");

    for(u_int32_t i=0;i<this->dumpers.size();++i){
        this->dumpers[i]->asynchronousStop();
        this->wbuffers[i]->asynchronousStop();
    }

    // printf("c\n");

    for(u_int32_t i=0;i<this->dumpers.size();++i){
        this->dumperThreads[i]->join();
    }

    // for(auto wb: this->wbuffers){
    //     wb->asynchronousStop();
    // }
}
void Controller::clear(){
    if(this->dumpers.size()!=0){
        for(u_int32_t i=0;i<this->dumpers.size();++i){
            delete this->dumpers[i];
            delete this->dumperThreads[i];
        }
        this->dumpers.clear();
        this->dumperThreads.clear();
    }

    if(this->checkers.size()!=0){
        for(u_int32_t i=0;i<this->checkers.size();++i){
            delete this->checkers[i];
            delete this->checkerThreads[i];
        }
        this->checkers.clear();
        this->checkerThreads.clear();
    }

    if(this->engines.size()!=0){
        for(u_int32_t i=0;i<this->engines.size();++i){
            delete this->engines[i];
            delete this->wbuffers[i];
            delete this->wbufferRings[i];
            delete this->indexRings[i];
            delete this->flow_maps[i];
        }
        this->engines.clear();
        this->wbuffers.clear();
        this->wbufferRings.clear();
        this->indexRings.clear();
        this->flow_maps.clear();
    }

    if(this->mem_pool != nullptr){
        delete this->mem_pool;
        this->mem_pool = nullptr;
    }

    if(this->dpdk != nullptr){
        delete this->dpdk;
        this->dpdk = nullptr;
    }
}

void Controller::init(InitData init_data){
    if(init_data.bind_core){
        this->bindCore(init_data.controller_core_id);
    }

    this->data_disk_fd = open(init_data.data_disk_name.c_str(), O_DIRECT | O_RDWR);
    this->data_disk_offset = init_data.data_disk_offset;
    if(this->data_disk_fd < 0){
        printf("Controller error: open data disk %s failed.\n", init_data.data_disk_name.c_str());
        throw std::runtime_error("open data disk failed");
    }

    if(init_data.data_disk_size % init_data.data_block_size != 0){
        printf("Controller error: data disk size %lu is not multiple of data block size %lu.\n",init_data.data_disk_size,init_data.data_block_size);
        throw std::runtime_error("data disk size is not multiple of data block size");
    }

    this->data_disk_size = init_data.data_disk_size;
    this->data_block_size = init_data.data_block_size;

    for(u_int64_t i=0;i<init_data.nb_rx;++i){
        PointerRingBuffer* wr = new PointerRingBuffer(init_data.flow_ring_capacity);
        this->wbufferRings.push_back(wr);
        PointerRingBuffer* ir = new PointerRingBuffer(1024);
        this->indexRings.push_back(ir);
    }

    this->mem_pool = new PointerRingPool(init_data.memory_pool_capacity,1024lu*4lu);

    for(u_int64_t i=0;i<init_data.nb_rx;++i){
        WBuffer* wb = new WBuffer(init_data.wbuffer_size_each,init_data.data_block_size);
        this->wbuffers.push_back(wb);
    }

    for(u_int64_t i=0;i<init_data.nb_rx;++i){
        std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash>* fm = new std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash>();
        this->flow_maps.push_back(fm);
    }

    this->dpdk = new DPDK(init_data.nb_rx,1,init_data.bind_core,init_data.dpdk_core_id_list);

    for(u_int64_t i=0;i<init_data.nb_rx;++i){
        Dumper* dp;
        if(init_data.bind_core){
            dp = new Dumper(this->wbuffers[i],this->data_disk_fd,this->data_disk_offset,this->data_disk_size,init_data.bind_core,init_data.data_dumping_core_id_list[i]);
        }else{
            dp = new Dumper(this->wbuffers[i],this->data_disk_fd,this->data_disk_offset,this->data_disk_size);
        }
        this->dumpers.push_back(dp);
    }

    for(u_int64_t i=0;i<init_data.checker_thread_num;++i){
        FlowChecker* fc;
        if(init_data.bind_core){
            fc = new FlowChecker(init_data.flow_buffer_time_threshold,init_data.bind_core,init_data.checking_core_id_list[i]);
        }else{
            fc = new FlowChecker(init_data.flow_buffer_time_threshold);
        }
        for(u_int64_t j=0;j<init_data.nb_rx;j+=init_data.checker_thread_num){
            fc->addCheckMap(this->flow_maps[j],this->wbufferRings[j]);
        }
        this->checkers.push_back(fc);
    }

    for(u_int64_t i=0;i<init_data.nb_rx;++i){
        FlowEngine* fe;
        if(init_data.bind_core){
            fe = new FlowEngine(init_data.eth_header_len,init_data.flow_buffer_len_threshold,init_data.flow_buffer_time_threshold,dpdk,0,i,this->flow_maps[i],this->wbuffers[i],this->mem_pool,this->wbufferRings[i],this->indexRings[i],init_data.bind_core,init_data.packet_core_id_list[i]);
        }else{
            fe = new FlowEngine(init_data.eth_header_len,init_data.flow_buffer_len_threshold,init_data.flow_buffer_time_threshold,dpdk,0,i,this->flow_maps[i],this->wbuffers[i],this->mem_pool,this->wbufferRings[i],this->indexRings[i]);
        }
        this->engines.push_back(fe);
    }

    printf("Detected logical cores: %u\n", rte_lcore_count());
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
    printf("wait.\n");

    char stop;

    std::cin>>stop;

    this->threadsStop();
}