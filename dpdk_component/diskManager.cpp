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

void DiskManager::addBlock(DiskBlock* block){
    // if (block->rss_id >= this->rss_count) {
    //     printf("Disk manager error: rss_id %u out of bounds!\n", block->rss_id);
    //     return;
    // }
    u_int32_t agent_id = block->block_id % this->agents.size();
    // if (block->write_pos >= this->block_num || block->last_write_pos >= this->block_num) {
    //     printf("Disk manager error: position %llu or last position %llu out of bounds!\n", block->write_pos, block->last_write_pos);
    //     return;
    // }
    this->agents[agent_id]->asyncWrite(block->buffer, block->block_id, block->write_pos);
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
    if (this->bind_core){
        this->bindCore();
    }
    this->stop = false;
    while(true){
        if(this->stop){
            break;
        }
        u_int64_t block_id = this->block_buffer->checkBlock(this->thread_id);
        // DiskBlock* block = (DiskBlock*)this->block_ring->get();
        if(block_id == std::numeric_limits<uint64_t>::max()){
            continue;
        }
        // u_int64_t disk_id = this->block_buffer->getDiskID(block_id);
        DiskBlock* block = this->block_buffer->getBlock(this->thread_id);
        
        this->disk_buffer->setTime(block->write_pos,block->start_time,block->end_time);
        this->disk_buffer->setPacketCount(block->write_pos,block->packet_count);
        // TDDO: Instantly write Packet Count?

        // this->setMeta(block);
        this->addBlock(block);
        delete block;
    }
    while (true){
        u_int64_t block_id = this->block_buffer->directGetBlockID(this->thread_id);
        if(block_id == std::numeric_limits<uint64_t>::max()){
            break;
        }
        DiskBlock* block = this->block_buffer->getBlock(this->thread_id);
        this->disk_buffer->setTime(block->write_pos,block->start_time,block->end_time);
        this->disk_buffer->setPacketCount(block->write_pos,block->packet_count);
        // TDDO: Instantly write Packet Count?

        this->addBlock(block);
        delete block;
    }
    
    printf("Disk manager log: thread quit.\n");
    return 0;
}

void DiskManager::asynchronousStop(){
    this->stop = true;
}