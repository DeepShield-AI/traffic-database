#include "diskManager.hpp"

// void DiskManager::setMeta(DiskBlock* block){
//     // if(block->write_pos >= this->block_num || block->last_write_pos >= this->block_num){
//     //     printf("Disk manager error: position %llu or last position %llu out of bounds!\n", block->write_pos, block->last_write_pos);
//     //     return;
//     // }
//     this->disk_buffer->setRSSID(block->write_pos, block->rss_id);
//     this->disk_buffer->setNextID(block->last_write_pos, block->write_pos);
//     this->disk_buffer->setTime(block->write_pos,block->start_time,block->end_time);
// }

void DiskManager::addBlock(void* block){
    if (this->agent_type == AgentType::DATA_AGENT){
        DataBlock* data_block = (DataBlock*)block;
        u_int32_t agent_id = data_block->block_id % this->agents.size();

        this->agents[agent_id]->asyncWrite(data_block->buffer, data_block->block_id, data_block->write_pos);
    }else if (this->agent_type == AgentType::INDEX_AGENT){
        IndexBlock* index_block = (IndexBlock*)block;
        u_int32_t agent_id = index_block->block_id % this->agents.size();

        this->agents[agent_id]->asyncWrite(index_block->buffer, index_block->block_id, index_block->write_pos);
    }
}

void DiskManager::addAgent(DiskAgent* agent){
    if (agent == nullptr) {
        printf("Disk manager error: agent is null!\n");
        return;
    }
    this->agents.push_back(agent);
}

void DiskManager::bindCore(){
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

void DiskManager::runData(){
    DataBlockBuffer* buffer = (DataBlockBuffer*)this->block_buffer;
    while(true){
        if(this->stop){
            break;
        }
        // printf("checking.\n");
        u_int64_t block_id = buffer->checkBlock(this->thread_id);
        if(block_id == std::numeric_limits<uint64_t>::max()){
            // printf("checking.\n");
            continue;
        }
        printf("get one.\n");
        // u_int64_t disk_id = this->block_buffer->getDiskID(block_id);
        DataBlock* block = buffer->getBlock(this->thread_id);
        
        this->disk_buffer->setTime(block->write_pos,block->start_time,block->end_time);
        this->disk_buffer->setPacketCount(block->write_pos,block->packet_count);
        // TDDO: Instantly write Packet Count?

        // this->setMeta(block);
        this->addBlock((void*)block);
        delete block;
    }
    while (true){
        u_int64_t block_id = buffer->directGetBlockID(this->thread_id);
        if(block_id == std::numeric_limits<uint64_t>::max()){
            break;
        }
        DataBlock* block = buffer->getBlock(this->thread_id);
        this->disk_buffer->setTime(block->write_pos,block->start_time,block->end_time);
        this->disk_buffer->setPacketCount(block->write_pos,block->packet_count);
        // TDDO: Instantly write Packet Count?

        this->addBlock((void*)block);
        delete block;
    }
}

void DiskManager::runIndex(){
    IndexBlockBuffer* buffer = (IndexBlockBuffer*)this->block_buffer;
    while(true){
        if(this->stop){
            break;
        }
        // printf("checking.\n");
        u_int64_t block_id = buffer->checkBlock(this->thread_id);
        if(block_id == std::numeric_limits<uint64_t>::max()){
            continue;
        }
        IndexBlock* block = buffer->getBlock(this->thread_id);

        this->addBlock((void*)block);
        delete block;
    }
}

void DiskManager::setBindCore(u_int32_t core_id){
    this->core_id = core_id;
    this->bind_core = true;
}

void DiskManager::setThreadID(u_int64_t thread_id){
    this->thread_id = thread_id;
}

int DiskManager::run(){
    if (this->thread_id == std::numeric_limits<uint64_t>::max()){
        printf("Disk manager error: run without thread id!\n");
        return -1;
    }

    printf("Disk manager log: thread run.\n");

    if (this->bind_core){
        this->bindCore();
    }
    this->stop = false;
    
    if (this->agent_type == AgentType::DATA_AGENT){
        this->runData();
    }else if (this->agent_type == AgentType::INDEX_AGENT){
        this->runIndex();
    }
    
    printf("Disk manager log: thread quit.\n");
    return 0;
}

void DiskManager::asynchronousStop(){
    this->stop = true;
}