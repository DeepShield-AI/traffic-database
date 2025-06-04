#include "engine.hpp"


void FlowEngine::readPacket(struct rte_mbuf *buf,u_int64_t ts,ParsedPacket* packet, FlowMetadata* flow_meta){
    packet->timestamp = ts;
    
    packet->header = rte_pktmbuf_mtod(buf, const u_int8_t *);

    uint8_t version = (*(u_int8_t*)(packet->header + this->eth_header_len) >> 4) & 0x0F;
    if(version == 4){
        const struct ip_header* ip_protocol = (const struct ip_header *)(packet->header + this->eth_header_len);

        // if (ip_protocol->ip_protocol != IPPROTO_TCP && ip_protocol->ip_protocol != IPPROTO_UDP){
        if (ip_protocol->ip_protocol != IPPROTO_TCP){
            packet = nullptr;
            flow_meta = nullptr;
            return;
        }
        
        const u_int16_t* sport = (const u_int16_t*)(packet->header + this->eth_header_len + ip_protocol->ip_header_length * 4);
        const u_int16_t* dport = sport + 1;
        u_int32_t srcip = htonl(ip_protocol->ip_source_address);
        u_int32_t dstip = htonl(ip_protocol->ip_destination_address);
        *flow_meta = {
            .sourceAddress = std::string((char*)&srcip,sizeof(srcip)),
            .destinationAddress = std::string((char*)&dstip,sizeof(dstip)),
            .sourcePort = htons(*sport),
            .destinationPort = htons(*dport),
        };

        u_int8_t l4_length = 0;
        if (ip_protocol->ip_protocol == IPPROTO_TCP) {
            l4_length = ((packet->header + this->eth_header_len + ip_protocol->ip_header_length * 4)[12] >> 4) * 4;
        }
        // } else if (ip_protocol->ip_protocol == IPPROTO_UDP) {
        //     l4_length = UDP_HEADER_LEN;
        // }
        packet->header_length = this->eth_header_len + ip_protocol->ip_header_length * 4 + l4_length;
    }else if(version == 6){
        u_int8_t l4_protocol = (packet->header + this->eth_header_len)[6];

        // if (l4_protocol != IPPROTO_TCP && l4_protocol != IPPROTO_UDP) {
        if (l4_protocol != IPPROTO_TCP) {
            packet = nullptr;
            flow_meta = nullptr;
            return;
        }

        const u_int16_t* sport = (const u_int16_t*)(packet->header + this->eth_header_len + IPV6_HEADER_LEN);
        const u_int16_t* dport = sport + 1;
        IPv6Address srcip = {
            .high = swap_endianness(*(u_int64_t*)(packet->header + this->eth_header_len + 8)),
            .low = swap_endianness(*(u_int64_t*)(packet->header + this->eth_header_len + 16)),
        };
        IPv6Address dstip = {
            .high = swap_endianness(*(u_int64_t*)(packet->header + this->eth_header_len + 24)),
            .low = swap_endianness(*(u_int64_t*)(packet->header + this->eth_header_len + 32)),
        };
        *flow_meta = {
            .sourceAddress = std::string((char*)&srcip,sizeof(srcip)),
            .destinationAddress = std::string((char*)&dstip,sizeof(dstip)),
            .sourcePort = htons(*sport),
            .destinationPort = htons(*dport),
        };

        u_int8_t l4_length = 0;
        if (l4_protocol == IPPROTO_TCP) {
            l4_length = ((packet->header + this->eth_header_len + IPV6_HEADER_LEN)[12] >> 4) * 4;
        }
        // } else if (l4_protocol == IPPROTO_UDP) {
        //     l4_length = UDP_HEADER_LEN;
        // }
        packet->header_length = this->eth_header_len + IPV6_HEADER_LEN + l4_length;
    }else{
        packet = nullptr;
        flow_meta = nullptr;
        return;
    }

    packet->data = packet->header + packet->header_length;
    packet->data_length = rte_pktmbuf_data_len(buf) - packet->header_length;

}

FlowBuffer* FlowEngine::writePacketToMap(ParsedPacket& packet, FlowMetadata& flow_meta){
    auto it = flow_map.find(flow_meta);
    FlowBuffer* buffer = nullptr;
    if (it != flow_map.end()){
        buffer = it->second;
        packet.is_upstream = true;
    }else{
        FlowMetadata ob_flow_meta = {
            .sourceAddress = flow_meta.destinationAddress,
            .destinationAddress = flow_meta.sourceAddress,
            .sourcePort = flow_meta.destinationPort,
            .destinationPort = flow_meta.sourcePort,
        };
        it = flow_map.find(ob_flow_meta);
        if (it != flow_map.end()){
            buffer = it->second;
            packet.is_upstream = false;
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
        flow_map[flow_meta] = buffer;
        return nullptr;
    }
    if (buffer->get_totol_length() + packet.data_length + packet.header_length < this->flow_buffer_len_threshold && buffer->is_finished()){
        if (!buffer->append_packet(&packet)){
            printf("Failed to append packet to exist flow buffer\n");
        }
        if (buffer->is_finished()){
            this->flow_map.erase(it->first);
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
    flow_map[flow_meta] = new_buffer;
    return buffer;
}

void FlowEngine::writeBufferToList(FlowBuffer* buffer){
    if(!this->flow_to_dump_list->put(buffer)){
        printf("Failed to put flow buffer to dump list\n");
        delete buffer;
    }
}