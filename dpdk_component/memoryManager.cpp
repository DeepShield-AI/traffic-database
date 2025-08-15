#include "memoryManager.hpp"


void MemoryManager::addAgent(DiskAgent* agent){
    if (agent == nullptr) {
        printf("Disk manager error: agent is null!\n");
        return;
    }
    this->disk_agents.push_back(agent);
}

void MemoryManager::bindCore(){
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(this->core_id, &cpuset);

    pthread_t thread = pthread_self();

    int set_result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (set_result != 0) {
        std::cerr << "Error setting thread affinity: " << set_result << std::endl;
    }

    // 确认设置是否成功
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    if (CPU_ISSET(this->core_id, &cpuset)) {
        printf("Disk manager log: %lu bind to cpu %d.\n",thread,this->core_id);
    } else {
        printf("Disk manager warning: %lu failed to bind to cpu %d!\n",thread,this->core_id);
    }
}

void MemoryManager::setBindCore(u_int32_t core_id){
    this->core_id = core_id;
    this->bind_core = true;
}

int MemoryManager::run(){
    printf("Memory manager log: thread run.\n");

    if (this->bind_core){
        this->bindCore();
    }
    this->stop = false;

    while(!this->stop){
        for(auto agent: this->disk_agents){
            u_int64_t block_id = agent->processCompletions();
            if (block_id == std::numeric_limits<u_int64_t>::max()) {
                continue;
            }
            printf("Memory manager log: type %u finish block %lu.\n",this->agent_type,block_id);
            if (this->agent_type == DATA_AGENT){
                DataBlockBuffer* buffer = (DataBlockBuffer*)this->block_buffer;
                buffer->recycleBlock((u_int64_t)block_id);
            }else if (this->agent_type == INDEX_AGENT){
                IndexBlockBuffer* buffer = (IndexBlockBuffer*)this->block_buffer;
                buffer->recycleBlock((u_int64_t)block_id);
            }
        }
    }
    for(auto agent: this->disk_agents){
        while(!agent->jobFinished()){
            u_int32_t block_id = agent->processCompletions();
        }
    }
    printf("Memory manager log: thread quit.\n");
    return 0;
}

void MemoryManager::asynchronousStop(){
    this->stop = true;
}