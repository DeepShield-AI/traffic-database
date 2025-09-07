#include "engine.hpp"

static uint64_t swap_endianness(uint64_t value) {
    return ((value >> 56) & 0x00000000000000FFULL) | // byte 0
           ((value >> 40) & 0x000000000000FF00ULL) | // byte 1
           ((value >> 24) & 0x00000000FF000000ULL) | // byte 2
           ((value >> 8)  & 0x00FF000000000000ULL) | // byte 3
           ((value << 8)  & 0xFF00000000000000ULL) | // byte 4
           ((value << 24) & 0x0000FF0000000000ULL) | // byte 5
           ((value << 40) & 0x000000FF00000000ULL) | // byte 6
           ((value << 56) & 0x00000000000000FFULL);   // byte 7
}

void FlowEngine::readPacket(struct rte_mbuf *buf,u_int64_t ts,ParsedPacket& packet, FlowMetadata& flow_meta){
    packet.timestamp = ts;
    
    packet.header = (char*)rte_pktmbuf_mtod(buf, const u_int8_t *);

    uint8_t version = (*(u_int8_t*)(packet.header + this->eth_header_len) >> 4) & 0x0F;
    if(version == 4){
        const struct ip_header* ip_protocol = (const struct ip_header *)(packet.header + this->eth_header_len);

        if (ip_protocol->ip_protocol != IPPROTO_TCP && ip_protocol->ip_protocol != IPPROTO_UDP){
        // if (ip_protocol->ip_protocol != IPPROTO_TCP){
            packet.data = nullptr;
            packet.header = nullptr;
            return;
        }

        const u_int16_t* sport = (const u_int16_t*)(packet.header + this->eth_header_len + ip_protocol->ip_header_length * 4);
        const u_int16_t* dport = sport + 1;
        u_int32_t srcip = htonl(ip_protocol->ip_source_address);
        u_int32_t dstip = htonl(ip_protocol->ip_destination_address);
        flow_meta = {
            .sourceAddress = std::string((char*)&srcip,sizeof(srcip)),
            .destinationAddress = std::string((char*)&dstip,sizeof(dstip)),
            .sourcePort = htons(*sport),
            .destinationPort = htons(*dport),
        };

        u_int8_t l4_length = 0;
        if (ip_protocol->ip_protocol == IPPROTO_TCP) {
            packet.is_fin = *(const u_int8_t*)(packet.header + this->eth_header_len + ip_protocol->ip_header_length * 4 + 13) & 0x01;
            packet.is_rst = *(const u_int8_t*)(packet.header + this->eth_header_len + ip_protocol->ip_header_length * 4 + 13) & 0x04;
            l4_length = ((*(packet.header + this->eth_header_len + ip_protocol->ip_header_length * 4 + 12)) >> 4) * 4;
        }else if (ip_protocol->ip_protocol == IPPROTO_UDP) {
            l4_length = UDP_HEADER_LEN;
        }
        packet.header_length = ip_protocol->ip_header_length * 4 + l4_length;
    }else if(version == 6){
        u_int8_t l4_protocol = (packet.header + this->eth_header_len)[6];

        if (l4_protocol != IPPROTO_TCP && l4_protocol != IPPROTO_UDP) {
        // if (l4_protocol != IPPROTO_TCP) {
            packet.header = nullptr;
            packet.data = nullptr;
            return;
        }

        const u_int16_t* sport = (const u_int16_t*)(packet.header + this->eth_header_len + IPV6_HEADER_LEN);
        const u_int16_t* dport = sport + 1;
        IPv6Address srcip = {
            .low = swap_endianness(*(u_int64_t*)(packet.header + this->eth_header_len + 16)),
            .high = swap_endianness(*(u_int64_t*)(packet.header + this->eth_header_len + 8)),
        };
        IPv6Address dstip = {
            .low = swap_endianness(*(u_int64_t*)(packet.header + this->eth_header_len + 32)),
            .high = swap_endianness(*(u_int64_t*)(packet.header + this->eth_header_len + 24)),
        };
        flow_meta = {
            .sourceAddress = std::string((char*)&srcip,sizeof(srcip)),
            .destinationAddress = std::string((char*)&dstip,sizeof(dstip)),
            .sourcePort = htons(*sport),
            .destinationPort = htons(*dport),
        };

        u_int8_t l4_length = 0;
        if (l4_protocol == IPPROTO_TCP) {
            packet.is_fin = *(const u_int8_t*)(packet.header + this->eth_header_len + IPV6_HEADER_LEN + 13) & 0x01;
            packet.is_rst = *(const u_int8_t*)(packet.header + this->eth_header_len + IPV6_HEADER_LEN + 13) & 0x04;
            l4_length = (*(packet.header + this->eth_header_len + IPV6_HEADER_LEN + 12) >> 4) * 4;
            
        }else if (l4_protocol == IPPROTO_UDP) {
            l4_length = UDP_HEADER_LEN;
        }
        packet.header_length = IPV6_HEADER_LEN + l4_length;
    }else{
        packet.header = nullptr;
        packet.data = nullptr;
        return;
    }
    packet.header = packet.header + this->eth_header_len;
    packet.data = packet.header + packet.header_length;
    packet.data_length = rte_pktmbuf_data_len(buf);
    if(packet.header_length > packet.data_length){
        // printf("something wrong.\n");
        packet.header_length = packet.data_length;
        packet.data_length = 0;
    }else{
        packet.data_length -= packet.header_length;
    }
}

FlowBuffer* FlowEngine::writePacketToMap(ParsedPacket& packet, FlowMetadata& flow_meta){
    auto it = flow_map->find(flow_meta);
    FlowBuffer* buffer = nullptr;
    if (it != flow_map->end()){
        buffer = it->second;
        packet.is_upstream = true;
    }else{
        FlowMetadata ob_flow_meta = {
            .sourceAddress = flow_meta.destinationAddress,
            .destinationAddress = flow_meta.sourceAddress,
            .sourcePort = flow_meta.destinationPort,
            .destinationPort = flow_meta.sourcePort,
        };
        it = flow_map->find(ob_flow_meta);
        if (it != flow_map->end()){
            buffer = it->second;
            packet.is_upstream = false;
            flow_meta = ob_flow_meta;
        }
    }
    if (buffer == nullptr){
        buffer = new FlowBuffer(this->pool);
        packet.is_upstream = true;
        if (!buffer->append_packet(&packet)){
            printf("Failed to append packet to new flow buffer\n");
            delete buffer;
            return nullptr;
        }
        (*flow_map)[flow_meta] = buffer;
        return nullptr;
    }
    if (buffer->get_totol_length() + packet.data_length + packet.header_length < this->flow_buffer_len_threshold && !buffer->is_finished() && !buffer->threshold_finished(this->flow_buffer_time_threshold,packet.timestamp)){
        if (!buffer->append_packet(&packet)){
            printf("Failed to append packet to exist flow buffer\n");
        }
        if (buffer->is_finished()){
            this->flow_map->erase(it->first);
            return buffer;
        }
        return nullptr;
    }
    FlowBuffer* new_buffer = new FlowBuffer(this->pool);
    packet.is_upstream = true;
    if (!buffer->is_finished()){
        new_buffer->set_flags(buffer->get_fin_seen(), buffer->get_rst_seen(), buffer->get_count_after_fin());
    }
    if (!new_buffer->append_packet(&packet)){
        printf("Failed to append packet to new flow buffer\n");
        delete new_buffer;
        return nullptr;
    }
    (*flow_map)[flow_meta] = new_buffer;
    return buffer;
}

void FlowEngine::writeFlowtoWBuffer(FlowBuffer* buffer){
    AggBuffer* timestamp_buffer = buffer->get_timestamp_buffer();
    AggBuffer* length_buffer = buffer->get_length_buffer();
    AggBuffer* header_buffer = buffer->get_header_buffer();
    AggBuffer* upstream_data_buffer = buffer->get_upstream_data_buffer();
    AggBuffer* downstream_data_buffer = buffer->get_downstream_data_buffer();

    AggBuffer* agg_buffers[] = {timestamp_buffer, length_buffer, header_buffer, upstream_data_buffer, downstream_data_buffer};

    for(auto buffer_ptr : agg_buffers){
        this->wbuffer->put(buffer_ptr);
    }
    delete buffer;
}

void FlowEngine::bindCore(u_int32_t cpu){
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
        printf("Flow engine log: %lu bind to cpu %d.\n",thread,cpu);
    } else {
        printf("Flow engine warning: %lu failed to bind to cpu %d!\n",thread,cpu);
    }
}

int FlowEngine::run(){

    if(this->bind_core){
        this->bindCore(this->core_id);
    }

    std::cout << "Flow engine log: thread run." << std::endl;

    this->stop = false;

    struct rte_mbuf *bufs[BURST_SIZE];
    int nb_rx;
    u_int64_t ts;
    u_int64_t pkt_count = 0;
    u_int64_t index_count = 0;
    u_int64_t byte_len = 0;

    auto start = std::chrono::high_resolution_clock::now();
    bool has_start = false;
    FlowMetadata flowMeta;
    ParsedPacket packet;

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

        for(int i=0;i<nb_rx;++i){
            pkt_count ++;
            this->readPacket(bufs[i],ts,packet,flowMeta);
            if(packet.header == nullptr || packet.data == nullptr){
                // printf("Flow engine error: Non-IP L3 protocol!\n");
                rte_pktmbuf_free(bufs[i]);
                continue;
            }

            byte_len += packet.data_length + packet.header_length;
            
            FlowBuffer* buffer = this->writePacketToMap(packet,flowMeta);
            if (buffer != nullptr){
                this->writeFlowtoWBuffer(buffer);
            }

            packet.header = nullptr;
            packet.data = nullptr;
            rte_pktmbuf_free(bufs[i]);
        }

        while(true){
            auto kv = (std::pair<FlowMetadata,FlowBuffer*>*)this->flowToWbufferRing->get();
            if(kv==nullptr){
                break;
            }
            auto it = this->flow_map->find(kv->first);
            if (it != flow_map->end()){
                printf("Flow engine warning: error kv.\n");
                continue;
            }
            this->flow_map->erase(it);
            this->writeFlowtoWBuffer(kv->second);
        }
        
        nb_rx = 0;
        if(this->stop){
            std::cout << "Flow engine log: asynchronous stop." << std::endl;
            break;
        }
    }

    for (auto& kv: *(this->flow_map)){
        this->writeFlowtoWBuffer(kv.second);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    this->duration_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    printf("DPDK Reader log: thread quit, during %lu us with %lu packets, %lu Bytes, %lu indexes, rate %f Gbps.\n",this->duration_time,pkt_count,byte_len,index_count,(double)byte_len/(double)this->duration_time/125.0);
    return 0;
}

void FlowEngine::asynchronousStop(){
    this->stop = true;
    // this->indexRing->asynchronousStop();
}