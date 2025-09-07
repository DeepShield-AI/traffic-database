#include "checker.hpp"



void FlowChecker::bindCore(u_int32_t cpu){
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
        printf("Flow checker log: %lu bind to cpu %d.\n",thread,cpu);
    } else {
        printf("Flow checker warning: %lu failed to bind to cpu %d!\n",thread,cpu);
    }
}

void FlowChecker::check(u_int64_t i){
    u_int64_t ts = rte_rdtsc();
    auto map_cpy = *(this->flow_maps[i]);
    for(auto& kv: map_cpy){
        if(kv.second->threshold_finished(this->flow_buffer_time_threshold,ts)){
            kv.second->set_finished();
            this->flowToWbufferRings[i]->put(&kv);
        }
    }
}

void FlowChecker::addCheckMap(std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash>* map, PointerRingBuffer* ring){
    this->flow_maps.push_back(map);
    this->flowToWbufferRings.push_back(ring);
}
int FlowChecker::run(){
    if(this->bind_core){
        this->bindCore(this->core_id);
    }
    this->stop = false;
    while(!this->stop){
        for(u_int64_t i = 0; i< this->flow_maps.size();++i){
            this->check(i);
        }
    }  
    return 0;
}
void FlowChecker::asynchronousStop(){
    this->stop = true;
}