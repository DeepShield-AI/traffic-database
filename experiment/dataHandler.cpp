#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

#include "../dpdk_lib/packetAggregator.hpp"
#include "../dpdk_lib/header.hpp"
#include "../dpdk_lib/util.hpp"

#define PAGE_SIZE 4096
#define BUFFER_SIZE 1024lu*1024lu*1024lu
#define ETH_HEADER_LEN 14
#define INDEX_ENUM_LEN sizeof(IndexStore)
#define DATA_OFFSET BUFFER_SIZE*32
#define INDEX_OFFSET BUFFER_SIZE*48
// #define DATA_OFFSET BUFFER_SIZE*64
// #define INDEX_OFFSET BUFFER_SIZE*80

struct PacketMeta{
    array_list_header* header;
    const char* data;
    u_int32_t len;
};


struct IndexStore{
    u_int64_t ts;
    u_int64_t disk_block_id;
    u_int64_t position;
    u_int64_t rx_id;
    char sourceAddress[16];
    char destinationAddress[16];
    u_int16_t sourcePort;
    u_int16_t destinationPort;
    u_int32_t version;
};

PacketAggregator* agg;

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

off_t get_file_size(const char *filename) {
    struct stat st;
    if (stat(filename, &st) == -1) {
        perror("stat");
        return -1;
    }
    return st.st_size;
}

void read_aligned(int fd, u_int64_t offset, char* buf, u_int64_t size) {
    printf("size:%lu\n",size);
    ssize_t ret = pread(fd, buf, size, offset);
    if (ret < 0) perror("pread");
}

void write_aligned(int fd, u_int64_t offset, char* buf, u_int64_t size) {
    ssize_t ret = pwrite(fd, buf, size, offset);
    if (ret < 0) perror("pwrite");
}

u_int64_t readPacket(char* buf, PacketMeta* meta, u_int64_t offset){
    meta->header = (array_list_header*)(buf + offset);
    meta->header->flow_next_diff = std::numeric_limits<uint32_t>::max();
    meta->data = buf + offset + sizeof(array_list_header);
    return offset + (u_int64_t)meta->header->caplen + (u_int64_t)sizeof(array_list_header);
}

FlowMetadata getFlowMetaData(PacketMeta& meta){
    uint8_t version = (*(u_int8_t*)(meta.data + ETH_HEADER_LEN) >> 4) & 0x0F;
    if(version == 4){
        const struct ip_header* ip_protocol = (const struct ip_header *)(meta.data + ETH_HEADER_LEN);
        const u_int16_t* sport = (const u_int16_t*)(meta.data + ETH_HEADER_LEN + ip_protocol->ip_header_length * 4);
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
        const u_int16_t* sport = (const u_int16_t*)(meta.data + ETH_HEADER_LEN + IPV6_HEADER_LEN);
        const u_int16_t* dport = sport + 1;
        IPv6Address srcip = {
            .low = swap_endianness(*(u_int64_t*)(meta.data + ETH_HEADER_LEN + 16)),
            .high = swap_endianness(*(u_int64_t*)(meta.data + ETH_HEADER_LEN + 8)),
        };
        IPv6Address dstip = {
            .low = swap_endianness(*(u_int64_t*)(meta.data + ETH_HEADER_LEN + 32)),
            .high = swap_endianness(*(u_int64_t*)(meta.data + ETH_HEADER_LEN + 24)),
        };
        FlowMetadata flow_meta = {
            .sourceAddress = std::string((char*)&srcip,sizeof(srcip)),
            .destinationAddress = std::string((char*)&dstip,sizeof(dstip)),
            .sourcePort = htons(*sport),
            .destinationPort = htons(*dport),
        };
        return flow_meta;
    }
    printf("%u\n",version);
    FlowMetadata flow_meta = {
        .sourceAddress = std::string(),
        .destinationAddress = std::string(),
        .sourcePort = 0,
        .destinationPort = 0,
    };
    return flow_meta;
}

u_int64_t calDiff(u_int64_t offset, u_int64_t last_offset){
    return offset - last_offset;
}

u_int64_t writeIndexToBuffer(u_int64_t value, FlowMetadata& meta, u_int64_t ts, char* buffer, u_int64_t offset){

    IndexStore* index = (IndexStore*)(buffer + offset);
    index->ts = ts;
    index->position = value;
    memcpy(&index->sourceAddress,meta.sourceAddress.c_str(),meta.sourceAddress.size());
    memcpy(&index->destinationAddress,meta.destinationAddress.c_str(),meta.destinationAddress.size());
    index->sourcePort = meta.sourcePort;
    index->destinationPort = meta.destinationPort;
    index->disk_block_id = value / BUFFER_SIZE;
    index->rx_id = 0;
    index->version = meta.sourceAddress.size();
    offset += INDEX_ENUM_LEN;

    return offset;
}

void writeBefore(u_int32_t diff, u_int64_t last_offset, char* buffer){
    array_list_header* header = (array_list_header*)(buffer + last_offset);
    header->flow_next_diff = diff;
}

int main(){
    const char* infile = "./data/source/filled_wide10Mp.pcap";
    const char* outfile = "/dev/sdb";

    u_int64_t len = get_file_size(infile);

    int read_fd = open(infile, O_RDONLY);
    if (read_fd < 0) {
        perror("open for read");
        return -1;
    }

    int write_fd = open(outfile, O_WRONLY);
    if (write_fd < 0) {
        perror("open for write");
        return -1;
    }

    char* read_buffer = new char[len];

    char* index_buffer = new char[BUFFER_SIZE];

    // if (posix_memalign(&index_buffer, PAGE_SIZE, BUFFER_SIZE) != 0) {
    //     perror("posix_memalign");
    //     return 1;
    // }

    agg = new PacketAggregator(1024lu*1024lu*1024lu,std::numeric_limits<uint64_t>::max());

    u_int64_t read_offset = 24;
    u_int64_t index_offset = 0;

    u_int64_t totol_len = read_offset;

    PacketMeta meta = {
        .header = new array_list_header,
        .data = nullptr,
        .len = 0,
    };

    for (u_int64_t i=0;i<len;i+=BUFFER_SIZE){
        u_int64_t batch_len = i + BUFFER_SIZE > len ? len - i: BUFFER_SIZE;
        read_aligned(read_fd,i,read_buffer + i,batch_len);
    }
    // read_aligned(read_fd,2147479704,read_buffer + 2147479704,len - 2147479704);
    // u_int64_t read_offset = BUFFER_SIZE*4;
    // u_int64_t write_offset = DATA_OFFSET;
    printf("read done with len %lu.\n",len);

    u_int64_t packet_count = 0;
    u_int64_t nonIP_count = 0;

    while(true){
        if (totol_len >= len){
            break;
        }
        if (packet_count % 1000000 == 0){
            printf("packets %lu with len %lu\n",packet_count,totol_len);
        }
        u_int64_t new_offset = readPacket(read_buffer, &meta, read_offset);
        packet_count ++;
        totol_len += new_offset - read_offset;
        u_int64_t ts = ((u_int64_t)meta.header->ts_h << 32) | (u_int64_t)meta.header->ts_l;

        FlowMetadata flow_meta = getFlowMetaData(meta);
        if(flow_meta.sourceAddress.size() == 0){
            printf("Non-IP L3 protocol, %lu, %lu!\n",new_offset - read_offset,read_offset);
            return 3;
            // nonIP_count ++;
            // continue;
        }
        
        u_int64_t last = agg->addPacket(flow_meta,read_offset,ts,BUFFER_SIZE);

        if(last != std::numeric_limits<uint64_t>::max()){
            u_int32_t diff = (u_int32_t)calDiff(read_offset,last);
            writeBefore(diff,last,read_buffer);
        }else{
            u_int64_t new_index_offset = writeIndexToBuffer(read_offset,flow_meta,ts,index_buffer,index_offset);
            index_offset = new_index_offset;
            if(index_offset > BUFFER_SIZE){
                printf("index overflow!\n");
                return 2;
            }
        }

        read_offset = new_offset;       
    }

    printf("parse done.\n");

    for (u_int64_t i=0;i<len;i+=BUFFER_SIZE){
        u_int64_t batch_len = i + BUFFER_SIZE > len ? len - i: BUFFER_SIZE;
        write_aligned(write_fd,i + DATA_OFFSET,read_buffer + i,batch_len);
    }
    // write_aligned(write_fd, DATA_OFFSET, read_buffer, len);
    write_aligned(write_fd, INDEX_OFFSET, index_buffer, index_offset);

    printf("data len: %lu, index len: %lu, packet count: %lu\n",len,index_offset,packet_count);
    printf("non-IP count: %lu\n",nonIP_count);

    delete[] read_buffer;
    delete[] index_buffer;

    return 0;
}
