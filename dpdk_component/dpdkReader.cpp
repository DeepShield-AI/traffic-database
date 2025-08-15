#include "dpdkReader.hpp"
#include <regex>
#include <random>

const u_int8_t pcap_head[] = {0xd4,0xc3,0xb2,0xa1,0x02,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                            0xff,0xff,0x00,0x00,0x01,0x00,0x00,0x00};


uint64_t swap_endianness(uint64_t value) {
    return ((value >> 56) & 0x00000000000000FFULL) | // byte 0
           ((value >> 40) & 0x000000000000FF00ULL) | // byte 1
           ((value >> 24) & 0x00000000FF000000ULL) | // byte 2
           ((value >> 8)  & 0x00FF000000000000ULL) | // byte 3
           ((value << 8)  & 0xFF00000000000000ULL) | // byte 4
           ((value << 24) & 0x0000FF0000000000ULL) | // byte 5
           ((value << 40) & 0x000000FF00000000ULL) | // byte 6
           ((value << 56) & 0x00000000000000FFULL);   // byte 7
}

// void DPDKReader::replaceBlock(u_int64_t ts){
//     DiskBlock* new_block = (DiskBlock*)this->blockRecieveRing->get();
//     if(new_block == nullptr){
//         return;
//     }
//     u_int64_t writePos = (*(this->diskWritePos))++;
//     writePos %= this->block_num;
//     new_block->start_time = ts;
//     new_block->write_pos = writePos;
//     new_block->rss_id = (this->port_id << 8) + this->rx_id;
//     new_block->last_write_pos = this->block_num;

//     this->queue_head++;
//     this->queue_head %= this->queue_size;
//     DiskBlock* block = this->blockTmpQueue[this->queue_head];
//     if(block == nullptr){
//         block->end_time = ts;
//         new_block->last_write_pos = block->write_pos;
//         this->blockWriteRing->put((void*)block);
//     }
//     this->blockTmpQueue[this->queue_head] = new_block;
// }

// void DPDKReader::writePointerToBlock(const char* data, u_int32_t len, u_int64_t ts){
//     if(this->write_offset + len > this->block_size){
//         u_int32_t tmp = this->block_size - this->write_offset;
//         memcpy(this->blockTmpQueue[this->queue_head]->buffer + this->write_offset, data, tmp);
//         this->replaceBlock(ts);
//         memcpy(this->blockTmpQueue[this->queue_head]->buffer + this->write_offset, data, len - tmp);
//         this->write_offset = len - tmp;
//         return;
//     }
//     memcpy(this->blockTmpQueue[this->queue_head]->buffer + this->write_offset, data, len);
//     this->write_offset += len;
// }

void DPDKReader::writeBefore(const char* data, u_int32_t len, u_int64_t last_offset){
    // u_int64_t last_queue_id = last_offset >> 32;
    // u_int64_t offset = last_offset & 0xFFFFFFFF;
    // if (last_queue_id >= this->queue_size){
    //     printf("DPDK reader error: last queue id %lu is out of range!\n", last_queue_id);
    //     return;
    // }
    // if (offset + len > this->block_size){
    //     u_int32_t tmp = this->block_size - offset;
    //     memcpy(this->blockTmpQueue[last_queue_id]->buffer + offset, data, tmp);
    //     last_queue_id++;
    //     last_queue_id %= this->queue_size;
    //     memcpy(this->blockTmpQueue[last_queue_id]->buffer, data + tmp, len - tmp);
    //     return;
    // }
    // memcpy(this->blockTmpQueue[last_queue_id]->buffer + offset, data, len);
    this->block_buffer->writeBlock(data,len,last_offset,this->thread_id, false, 0);
    // printf("write before on %lu.\n",last_offset);
}

void DPDKReader::readPacket(struct rte_mbuf *buf, u_int64_t ts, PacketMeta* meta){
    // meta->tag_num = 0;
    // for(u_int8_t i = 0; i< MAX_TAG_NUM; ++i){
    //     Tag* tag = (Tag*)&(buf->dynfield1[i]);
    //     if(tag->id == 0){
    //         break;
    //     }
    //     meta->tag_num ++;
    // }

    // meta->tags = (Tag*)(buf->dynfield1);
    
    meta->header->flow_next_diff = std::numeric_limits<uint32_t>::max();
    meta->header->caplen = rte_pktmbuf_data_len(buf) - this->eth_header_len;
    meta->header->ts_l = (u_int32_t)(ts & 0xFFFFFFFF);
    meta->header->ts_h = (u_int32_t)(ts >> 32);

    meta->len = meta->header->caplen;
    meta->data = rte_pktmbuf_mtod(buf, const char *);
}

u_int64_t DPDKReader::writePacketToPacketBuffer(PacketMeta& meta, u_int64_t ts){
    // u_int64_t _offset = ((u_int64_t)(this->queue_head) << 32) + this->write_offset;
    u_int64_t total_len = sizeof(pcap_header) + meta.len;
    u_int64_t _offset = this->diskWritePos->fetch_add(total_len);
    _offset %= this->disk_size;
    // this->writePointerToBlock((const char*)meta.header,sizeof(pcap_header),ts);
    // this->writePointerToBlock(meta.data + this->eth_header_len, meta.len, ts);
    // if (!this->block_buffer->writeBlock((const char*)meta.header,sizeof(pcap_header),_offset,this->thread_id,true,ts)){
    //     return std::numeric_limits<uint64_t>::max();
    // }
    // if (!this->block_buffer->writeBlock(meta.data + this->eth_header_len, meta.len, _offset + sizeof(pcap_header), this->thread_id, true, ts)){
    //     return std::numeric_limits<uint64_t>::max();
    // }
    // return (u_int32_t)(this->packetBuffer->getFileOffset() + this->packetBuffer->getOffset()) - meta.len - sizeof(pcap_header);
    while (!this->block_buffer->writeBlock((const char*)meta.header,sizeof(pcap_header),_offset,this->thread_id,true,ts)){
        printf("DPDK reader warning: write pcap header to block buffer on %lu failed, retrying...\n", _offset);
    }
    while (!this->block_buffer->writeBlock(meta.data + this->eth_header_len, meta.len, _offset + sizeof(pcap_header), this->thread_id, true, ts)){
        printf("DPDK reader warning: write packet data to block buffer on %lu failed, retrying...\n", _offset + sizeof(pcap_header));
    }
    // printf("write packet on %lu.\n",_offset);
    return _offset;
}

FlowMetadata DPDKReader::getFlowMetaData(PacketMeta& meta){
    // printf("pkt len:%lu\n",meta.len);
    this->byteLen += meta.len;
    uint8_t version = (*(u_int8_t*)(meta.data + this->eth_header_len) >> 4) & 0x0F;
    if(version == 4){
        const struct ip_header* ip_protocol = (const struct ip_header *)(meta.data + this->eth_header_len);
        const u_int16_t* sport = (const u_int16_t*)(meta.data + this->eth_header_len + ip_protocol->ip_header_length * 4);
        const u_int16_t* dport = sport + 1;
        u_int32_t srcip = htonl(ip_protocol->ip_source_address);
        u_int32_t dstip = htonl(ip_protocol->ip_destination_address);
        FlowMetadata flow_meta = {
            .sourceAddress = std::string((char*)&srcip,sizeof(srcip)),
            .destinationAddress = std::string((char*)&dstip,sizeof(dstip)),
            .sourcePort = htons(*sport),
            .destinationPort = htons(*dport),
        };
        return flow_meta;
    }else if(version == 6){
        const u_int16_t* sport = (const u_int16_t*)(meta.data + this->eth_header_len + IPV6_HEADER_LEN);
        const u_int16_t* dport = sport + 1;
        IPv6Address srcip = {
            .low = swap_endianness(*(u_int64_t*)(meta.data + this->eth_header_len + 16)),
            .high = swap_endianness(*(u_int64_t*)(meta.data + this->eth_header_len + 8)),
        };
        IPv6Address dstip = {
            .low = swap_endianness(*(u_int64_t*)(meta.data + this->eth_header_len + 32)),
            .high = swap_endianness(*(u_int64_t*)(meta.data + this->eth_header_len + 24)),
        };
        FlowMetadata flow_meta = {
            .sourceAddress = std::string((char*)&srcip,sizeof(srcip)),
            .destinationAddress = std::string((char*)&dstip,sizeof(dstip)),
            .sourcePort = htons(*sport),
            .destinationPort = htons(*dport),
        };
        return flow_meta;
    }
    FlowMetadata flow_meta = {
        .sourceAddress = std::string(),
        .destinationAddress = std::string(),
        .sourcePort = 0,
        .destinationPort = 0,
    };
    return flow_meta;
}

u_int64_t DPDKReader::calValue(u_int64_t _offset){
    // u_int64_t value = 0;
    // u_int64_t queue_id = _offset >> 32;
    // u_int64_t offset_in_block = _offset & 0xFFFFFFFF;
    // value |= this->blockTmpQueue[queue_id]->write_pos & 0xFFFFFFFF;
    // value <<= 32;
    // value |= offset_in_block;
    // value |= this->port_id & 0xff;
    // value <<= 8;
    // value |= this->rx_id & 0xff;
    // value <<= 48;
    // value |= _offset & 0xffffffffffff;
    // printf("offset:%lu.\n",_offset);
    return _offset;
}

u_int64_t DPDKReader::calDiff(u_int64_t offset, u_int64_t last_offset){
    // u_int64_t diff = 0;
    // u_int64_t last_queue_id = last_offset >> 32;
    // u_int64_t last_offset_in_block = last_offset & 0xFFFFFFFF;
    // u_int64_t queue_id = offset >> 32;
    // u_int64_t offset_in_block = offset & 0xFFFFFFFF;

    // if (last_queue_id <= queue_id){
    //     diff = (queue_id-last_queue_id) * this->block_size + offset_in_block - last_offset_in_block;
    //     return diff;
    // }
    // diff = (queue_id + this->queue_size - last_queue_id) * this->block_size + offset_in_block - last_offset_in_block;

    return (offset + this->disk_size - last_offset) % this->disk_size;
}

u_int64_t DPDKReader::calIndexNodeLen(u_int32_t key_len, u_int32_t level){
    // printf("keylen: %u, level: %u\n",key_len,level);
    u_int64_t len = sizeof(u_int64_t);
    len += key_len;
    len += sizeof(SpinLock);
    len += sizeof(u_int32_t);
    len += sizeof(void*) * level;
    return len;
}

bool DPDKReader::writeIndexToRing(u_int64_t value, FlowMetadata meta, u_int64_t ts){
    u_int32_t level = 0;
    Index* index = nullptr;

    index = new Index();
    // index->key = *(u_int32_t*)(meta.sourceAddress.c_str());
    // index->key = meta.sourceAddress;
    // index->value = value;
    index->ts = ts;
    index->id = meta.sourceAddress.size() == 4? IndexType::SRCIP:IndexType::SRCIPv6;
    index->disk_block_id = value / this->block_size;
    // index->len = meta.sourceAddress.size();
    level = SkipList::randomLevel(meta.sourceAddress.size()*8);
    index->len = this->calIndexNodeLen(meta.sourceAddress.size(), level);
    index->node = this->indexMemoryPool->allocate(index->len,value/this->block_size);
    if(index->id == IndexType::SRCIP){
        SkipListNode<u_int32_t,u_int64_t>* node = (SkipListNode<u_int32_t,u_int64_t>*)index->node;
        node->init(*(u_int32_t*)(meta.sourceAddress.c_str()), value, level);
    }else{
        SkipListNode<IPv6Address,u_int64_t>* node = (SkipListNode<IPv6Address,u_int64_t>*)index->node;
        node->init(*(IPv6Address*)(meta.sourceAddress.c_str()), value, level);
    }
    
    if(!(*(this->indexRings))[0]->put((void*)index)){
        return false;
    }

    // index = new Index();
    // // index->key =  *(u_int32_t*)(meta.destinationAddress.c_str());
    // index->key = meta.destinationAddress;
    // index->value = value;
    // index->ts = ts;
    // index->id = meta.destinationAddress.size() == 4? IndexType::DSTIP:IndexType::DSTIPv6;
    // index->len = meta.destinationAddress.size();

    index = new Index();
    index->ts = ts;
    index->id = meta.destinationAddress.size() == 4? IndexType::DSTIP:IndexType::DSTIPv6;
    index->disk_block_id = value / this->block_size;
    level = SkipList::randomLevel(meta.destinationAddress.size()*8);
    index->len = this->calIndexNodeLen(meta.destinationAddress.size(), level);
    index->node = this->indexMemoryPool->allocate(index->len,value/this->block_size);
    if(index->id == IndexType::DSTIP){
        SkipListNode<u_int32_t,u_int64_t>* node = (SkipListNode<u_int32_t,u_int64_t>*)index->node;
        node->init(*(u_int32_t*)(meta.destinationAddress.c_str()), value, level);
    }else{
        SkipListNode<IPv6Address,u_int64_t>* node = (SkipListNode<IPv6Address,u_int64_t>*)index->node;
        node->init(*(IPv6Address*)(meta.destinationAddress.c_str()), value, level);
    }
    if(!(*(this->indexRings))[0]->put((void*)index)){
        return false;
    }

    index = new Index();
    // index->key = meta.sourcePort;
    // index->key = std::string((char*)&(meta.sourcePort),sizeof(meta.sourcePort));
    // index->value = value;
    index->ts = ts;
    index->id = IndexType::SRCPORT;
    index->disk_block_id = value / this->block_size;
    // index->len = sizeof(meta.sourcePort);
    level = SkipList::randomLevel(sizeof(meta.sourcePort)*8);
    index->len = this->calIndexNodeLen(sizeof(meta.sourcePort), level);
    index->node = this->indexMemoryPool->allocate(index->len,value/this->block_size);
    SkipListNode<u_int16_t,u_int64_t>* node = (SkipListNode<u_int16_t,u_int64_t>*)index->node;
    node->init(meta.sourcePort, value, level);
    if(!(*(this->indexRings))[0]->put((void*)index)){
        return false;
    }

    index = new Index();
    // index->key = meta.destinationPort;
    // index->key = std::string((char*)&(meta.destinationPort),sizeof(meta.destinationPort));
    // index->value = value;
    // index->ts = ts;
    // index->id = IndexType::DSTPORT;
    // index->len = sizeof(meta.sourcePort);

    index->ts = ts;
    index->id = IndexType::DSTPORT;
    index->disk_block_id = value / this->block_size;
    level = SkipList::randomLevel(sizeof(meta.destinationPort)*8);
    index->len = this->calIndexNodeLen(sizeof(meta.destinationPort), level);
    index->node = this->indexMemoryPool->allocate(index->len,value/this->block_size);
    node = (SkipListNode<u_int16_t,u_int64_t>*)index->node;
    node->init(meta.destinationPort, value, level);
    if(!(*(this->indexRings))[0]->put((void*)index)){
        return false;
    }

    if (meta.sourceAddress.size() == 4){
        index = new Index();
        // index->key = meta.destinationPort;
        QuarTurpleIPv4 ipv4Turple = {
            .dstport = meta.destinationPort,
            .srcport = meta.sourcePort,
            .dstip = *(u_int32_t*)(meta.destinationAddress.c_str()),
            .srcip = *(u_int32_t*)(meta.sourceAddress.c_str()),
        };
        // index->key = std::string((char*)&(ipv4Turple),sizeof(ipv4Turple));
        // index->value = value;
        index->ts = ts;
        index->id = IndexType::QUARTURPLEIPv4;
        index->disk_block_id = value / this->block_size;
        // index->len = sizeof(ipv4Turple);
        level = SkipList::randomLevel(sizeof(ipv4Turple)*8);
        index->len = this->calIndexNodeLen(sizeof(ipv4Turple), level);
        index->node = this->indexMemoryPool->allocate(index->len,value/this->block_size);
        SkipListNode<QuarTurpleIPv4,u_int64_t>* node = (SkipListNode<QuarTurpleIPv4,u_int64_t>*)index->node;
        node->init(ipv4Turple, value, level);
        if(!(*(this->indexRings))[0]->put((void*)index)){
            return false;
        }
    }else{
        index = new Index();
        // index->key = meta.destinationPort;
        QuarTurpleIPv6 ipv6Turple = {
            .dstport = meta.destinationPort,
            .srcport = meta.sourcePort,
            .dstip = *(IPv6Address*)(meta.destinationAddress.c_str()),
            .srcip = *(IPv6Address*)(meta.sourceAddress.c_str()),
        };
        // index->key = std::string((char*)&(ipv6Turple),sizeof(ipv6Turple));
        // index->value = value;
        index->ts = ts;
        index->id = IndexType::QUARTURPLEIPv6;
        index->disk_block_id = value / this->block_size;
        // index->len = sizeof(ipv6Turple);
        level = SkipList::randomLevel(sizeof(ipv6Turple)*8);
        index->len = this->calIndexNodeLen(sizeof(ipv6Turple), level);
        index->node = this->indexMemoryPool->allocate(index->len,value/this->block_size);
        SkipListNode<QuarTurpleIPv6,u_int64_t>* node = (SkipListNode<QuarTurpleIPv6,u_int64_t>*)index->node;
        node->init(ipv6Turple, value, level);
        if(!(*(this->indexRings))[0]->put((void*)index)){
            return false;
        }
    }
    return true;
}

void DPDKReader::bindCore(u_int32_t cpu){
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
        printf("DPDK reader log: %lu bind to cpu %d.\n",thread,cpu);
    } else {
        printf("DPDK reader warning: %lu failed to bind to cpu %d!\n",thread,cpu);
    }
}

void DPDKReader::setThreadID(u_int64_t threadID){
    this->thread_id = threadID;
}

int DPDKReader::run(){
    // pcap file header
    
    // this->packetBuffer->writePointer((char*)pcap_head,this->pcap_header_len);

    if(this->thread_id == std::numeric_limits<uint64_t>::max()){
        printf("DPDK reader error: run without thread id!\n");
        return -1;
    }

    if(this->bind_core){
        // this->bindCore(this->rx_id*2 + 72);
        this->bindCore(this->core_id);
    }

    std::cout << "DPDK reader log: thread run." << std::endl;
    this->stop = false;
    
    u_int64_t truncate_time = 0;

    struct rte_mbuf *bufs[BURST_SIZE];
    int nb_rx;
    u_int64_t ts;
    u_int64_t pkt_count = 0;
    u_int64_t index_count = 0;
    auto start = std::chrono::high_resolution_clock::now();
    bool has_start = false;
    PacketMeta meta = {
        .header = new array_list_header,
        .data = nullptr,
        .len = 0,
    };

    u_int64_t read_time = 0;
    u_int64_t analysis_time = 0;
    u_int64_t write_time = 0;
    u_int64_t aggregate_time = 0;
    u_int64_t index_time = 0;
    u_int64_t delete_time = 0;
    u_int64_t total_time = 0;
    
    while(true){
        ts = rte_rdtsc();
        nb_rx = this->dpdk->getRXBurst(bufs,this->port_id,this->rx_id);
        
        if(nb_rx == 0 && !(this->stop)){
            continue;
        }
        if(!has_start){
            start = std::chrono::high_resolution_clock::now();
            has_start=true;
        }
        int err = 0;
        for(int i=0;i<nb_rx;++i){
            pkt_count ++;
            auto total_start = std::chrono::high_resolution_clock::now();

            auto read_start = std::chrono::high_resolution_clock::now();
            this->readPacket(bufs[i],ts,&meta);
            if(meta.data == nullptr){
                std::cout << "DPDK Reader log: read over." << std::endl;
                err = 1;
                break;
            }
            auto read_end = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(read_end - read_start).count();

            // for(u_int8_t i = 0; i< meta.tag_num; ++i){
                
            // }
            // printfauto read_start = std::chrono::high_resolution_clock::now();("\n");
            auto write_start = std::chrono::high_resolution_clock::now();
            u_int64_t _offset = this->writePacketToPacketBuffer(meta, ts);
            if(_offset == std::numeric_limits<uint64_t>::max()){
                printf("DPDK Reader warning: packet buffer overflow!\n");
                meta.data = nullptr;
                rte_pktmbuf_free(bufs[i]);
                continue;
            }
            auto write_end = std::chrono::high_resolution_clock::now();
            write_time += std::chrono::duration_cast<std::chrono::microseconds>(write_end - write_start).count();

            auto analysis_start = std::chrono::high_resolution_clock::now();
            FlowMetadata flow_meta = this->getFlowMetaData(meta);
            if(flow_meta.sourceAddress.size() == 0){
                printf("DPDK Reader error: Non-IP L3 protocol!\n");
                meta.data = nullptr;
                rte_pktmbuf_free(bufs[i]);
                continue;
            }
            auto analysis_end = std::chrono::high_resolution_clock::now();
            analysis_time += std::chrono::duration_cast<std::chrono::microseconds>(analysis_end - analysis_start).count();

            auto aggregate_start = std::chrono::high_resolution_clock::now();
            u_int64_t last = this->packetAggregator->addPacket(flow_meta,_offset,ts, this->disk_size);
            auto aggregate_end = std::chrono::high_resolution_clock::now();
            aggregate_time += std::chrono::duration_cast<std::chrono::microseconds>(aggregate_end - aggregate_start).count();

            auto index_start = std::chrono::high_resolution_clock::now();
            if(last != std::numeric_limits<uint64_t>::max()){
                // printf("%lu\n",last);
                u_int32_t diff = (u_int32_t)this->calDiff(_offset,last);
                this->writeBefore((const char*)(&diff),sizeof(diff),last);
            }else{
                /* with index */
                u_int64_t value = this->calValue(_offset);
                if(!this->writeIndexToRing(value,flow_meta,ts)){
                    printf("DPDK Reader error: write index to ring failed!\n");
                }
                index_count++;
            }
            auto index_end = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(index_end - index_start).count();
            
            

            // if(!this->writeTagToRing(meta.data,meta.tags,meta.tag_num,flow_meta,ts,_offset,last)){
            //     printf("DPDK Reader error: write tag to ring failed!\n");
            // }

            // printf("packet offset: %lu, l3 offset: %lu, l4 offset: %lu.\n",_offset,info->l3_offset,info->l4_offset);
            
            auto delete_start = std::chrono::high_resolution_clock::now();
            meta.data = nullptr;
            rte_pktmbuf_free(bufs[i]);
            auto delete_end = std::chrono::high_resolution_clock::now();
            delete_time += std::chrono::duration_cast<std::chrono::microseconds>(delete_end - delete_start).count();

            auto total_end = std::chrono::high_resolution_clock::now();
            total_time += std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start).count();
        }
        nb_rx = 0;
        if(err){
            break;
        }
        if(this->stop){
            std::cout << "DPDK Reader log: asynchronous stop." << std::endl;
            break;
        }
    }
    // if(!this->writeAllTagsToRing(ts)){
    //     printf("DPDK Reader error: write all tags to ring failed!\n");
    // }
    auto end = std::chrono::high_resolution_clock::now();

    this->duration_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printf("DPDK Reader log: thread quit, during %lu us with %lu packets, %lu Bytes, %lu indexes.\n",this->duration_time,pkt_count,this->byteLen,index_count);
    printf("DPDK Reader log: read time %lu us, write time %lu us,analysis time %lu us ,aggregate time %lu us, index time %lu us, delete time %lu us, total time %lu us.\n",read_time,write_time,analysis_time,aggregate_time,index_time,delete_time,total_time);
    return 0;
}

// struct TestIndex{
//     u_int32_t srcip;
//     u_int32_t dstip;
//     u_int16_t srcport;
//     u_int16_t dstport;
// };

// uint32_t ipToUint32(const std::string& ip) {
//     uint32_t result = 0;
//     inet_pton(AF_INET, ip.c_str(), &result); // 将IP转换为无符号整型
//     return ntohl(result); // 将网络字节序转换为主机字节序
// }

// int DPDKReader::run(){
//     std::string filename = "./data/source/flow.txt";
//     std::ifstream infile(filename);
//     std::string line;

//     std::vector<TestIndex> vec = std::vector<TestIndex>();

//     if (!infile.is_open()) {
//         std::cerr << "Error opening file: " << filename << std::endl;
//         return  -1;
//     }

//     std::regex flowRegex(R"(\('([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)',\s*(\d+),\s*'([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)',\s*(\d+)\):\s*(\d+))");
//     std::smatch match;

//     u_int32_t count = 0;

//     while (std::getline(infile, line)) {
//         if (std::regex_search(line, match, flowRegex) && match.size() == 6) {
//             std::string srcIp = match[1];
//             uint16_t srcPort = static_cast<uint16_t>(std::stoi(match[2]));
//             std::string dstIp = match[3];
//             uint16_t dstPort = static_cast<uint16_t>(std::stoi(match[4]));
//             size_t len = static_cast<size_t>(std::stoull(match[5]));

//             uint32_t srcIpInt = ipToUint32(srcIp);
//             uint32_t dstIpInt = ipToUint32(dstIp);

//             TestIndex id = {
//                 .srcip = srcIpInt,
//                 .dstip = dstIpInt,
//                 .srcport = srcPort,
//                 .dstport = dstPort,
//             };
//             vec.push_back(id);

//         }else {
//             std::cerr << "Line format error: " << line << std::endl;
//         }
//     }
//     infile.close();
//     auto start = std::chrono::high_resolution_clock::now();
//     u_int64_t id_count = 0;
//     for(u_int32_t i = 0;i<2;++i){
//         for(auto id:vec){
            
//             FlowMetadata flow_meta = {
//                 .sourceAddress = std::string((char*)&id.srcip,sizeof(u_int32_t)),
//                 .destinationAddress = std::string((char*)&id.dstip,sizeof(u_int32_t)),
//                 .sourcePort = id.srcport,
//                 .destinationPort = id.dstport,
//             };
//             id.srcip++;
//             id.dstip++;
//             id.srcport++;
//             id.dstport++;
//             if(!this->writeIndexToRing(id_count,flow_meta,0)){
//                 printf("DPDK Reader error: write index to ring failed!\n");
//             }
//             id_count ++;
//             // if(id_count >= 800000){
//             //     break;
//             // }
//         }
//     }
//     auto end = std::chrono::high_resolution_clock::now();
//     this->duration_time += std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
//     printf("DPDK Reader log: thread quit, during %lu us with %lu indexes.\n",this->duration_time,id_count);
//     return 0;
// }

void DPDKReader::asynchronousStop(){
    this->stop = true;
}
