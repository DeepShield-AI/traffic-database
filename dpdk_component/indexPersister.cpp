#include "indexPersister.hpp"

u_int64_t IndexPersister::bit_ceil(u_int32_t x){
    if (x <= 1) return 1;
    --x;
    for (size_t i = 1; i < sizeof(u_int32_t) * 8; i <<= 1) {
        x |= x >> i;
    }
    return ++x;
}

void IndexPersister::bindCore(u_int32_t cpu){
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
        printf("Index Persister log: %lu bind to cpu %d.\n",thread,cpu);
    } else {
        printf("Index Persister warning: %lu failed to bind to cpu %d!\n",thread,cpu);
    }
}

IndexBufferMeta* IndexPersister::checkAndGetMeta(){
    u_int64_t disk_block_id = this->indexBuffer->getCheckDishID(this->index_buffer_thread_id);
    u_int64_t packet_count = 0;
    if (disk_block_id == std::numeric_limits<u_int64_t>::max()){
        return nullptr;
    }
    while(true){
        if (this->stop){
            return nullptr;
        }
        packet_count = this->diskBuffer->getDiskMeta(disk_block_id)->packet_count;
        if (packet_count != 0){
            break;
        }
    }
    // printf("get block id %lu, packet_count %lu\n",disk_block_id,packet_count);
    while(true){
        u_int64_t srcip_count = this->indexBuffer->checkIndexCount(this->index_block_buffer_thread_id, IndexType::SRCIP) + this->indexBuffer->checkIndexCount(this->index_block_buffer_thread_id, IndexType::SRCIPv6);
        // printf("srcip count: %lu\n",srcip_count);
        if (srcip_count < packet_count){
            continue;
        }
        u_int64_t dstip_count = this->indexBuffer->checkIndexCount(this->index_block_buffer_thread_id, IndexType::DSTIP) + this->indexBuffer->checkIndexCount(this->index_block_buffer_thread_id, IndexType::DSTIPv6);
        if (dstip_count < packet_count){
            continue;
        }
        u_int64_t srcport_count = this->indexBuffer->checkIndexCount(this->index_block_buffer_thread_id, IndexType::SRCPORT);
        if (srcport_count < packet_count){
            continue;
        }
        u_int64_t dstport_count = this->indexBuffer->checkIndexCount(this->index_block_buffer_thread_id, IndexType::DSTPORT);
        if (dstport_count < packet_count){
            continue;
        }
        u_int64_t quartruple_count = this->indexBuffer->checkIndexCount(this->index_block_buffer_thread_id, IndexType::QUARTURPLEIPv4) + this->indexBuffer->checkIndexCount(this->index_block_buffer_thread_id, IndexType::QUARTURPLEIPv6);
        if (quartruple_count < packet_count){
            continue;
        }

        IndexBufferMeta* meta = this->indexBuffer->getIndexBufferMeta(this->index_block_buffer_thread_id);
        return meta;
    }
    return nullptr;
}

void IndexPersister::persistMeta(IndexBufferMeta* meta){
    u_int64_t total_node_len = 0;
    for (u_int32_t type = IndexType::SRCIP; type < IndexType::TOTAL_INDEX; ++type){
        total_node_len += meta->skiplists[type].getNodeNum()*(meta->skiplists[type].getKeyLen() + meta->skiplists[type].getValueLen());
    }
    u_int64_t disk_pos = this->diskWritePos->fetch_add(total_node_len);
    disk_pos = disk_pos % this->disk_size;

    u_int64_t current_offset = disk_pos;
    for (u_int32_t type = IndexType::SRCIP; type < IndexType::TOTAL_INDEX; ++type){
        meta->skiplists[type].writeNode(this->indexBlockBuffer, this->index_block_buffer_thread_id, current_offset);
        u_int64_t new_offset = current_offset + meta->skiplists[type].getNodeNum()*(meta->skiplists[type].getKeyLen() + meta->skiplists[type].getValueLen());
        this->diskBuffer->setIndexID(meta->disk_block_id, (IndexType)type, current_offset, new_offset);
        current_offset = new_offset;
    }

    this->diskBuffer->setBloomFilterCol(meta->disk_block_id, meta->bloomFilterMeta.getWritingCol());
}
void IndexPersister::clearMeta(IndexBufferMeta* meta){
    this->diskBuffer->clearPacketCount(meta->disk_block_id);
    for(auto pool : *(this->memoryPools)){
        pool->recycle(meta->disk_block_id);
    }
    for (u_int32_t type = IndexType::SRCIP; type < IndexType::TOTAL_INDEX; ++type){
        meta->skiplists[type].clear();
    }

    BitMap* bitmap = meta->bloomFilterMeta.getBitmap();
    u_int64_t backup_col = bitmap->getBackupColCount();
    u_int64_t new_col = meta->bloomFilterMeta.getWritingCol() + backup_col;
    meta->bloomFilterMeta.getBitmap()->clearCol(new_col);
    this->indexBuffer->updateIndexBufferMeta(this->index_buffer_thread_id, new_col);
}

// void IndexPersister::setThreadID(u_int64_t threadID){
//     this->thread_id = threadID;
// }

int IndexPersister::run(){
    if(this->index_buffer_thread_id == std::numeric_limits<uint64_t>::max() || this->index_block_buffer_thread_id == std::numeric_limits<uint64_t>::max()){
        printf("Index Persister error: run without thread id!\n");
        return -1;
    }

    if(this->bind_core){
        // this->bindCore(this->rx_id*2 + 72);
        this->bindCore(this->core_id);
    }

    std::cout << "Index Persister log: thread run." << std::endl;
    this->stop = false;

    while(true){
        IndexBufferMeta* meta = this->checkAndGetMeta();
        if(meta == nullptr){
            break;
        }
        this->persistMeta(meta);
        printf("Index Persister log: persist index of block %lu.\n",meta->disk_block_id);
        this->clearMeta(meta);
    }

    std::cout << "Index Persister log: thread quit." << std::endl;
    return 0;
}

void IndexPersister::asynchronousStop(){
    this->stop = true;
}