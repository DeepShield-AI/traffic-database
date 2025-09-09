#include "../dpdk_lib/prefixBloomFilter.hpp"
#include "../dpdk_lib/indexBlockBuffer.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <numa.h>
#include <numaif.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <random>
#include <chrono>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <set>
#include <numeric> 
#include <list>

#define QUERY_TIMES 100
#define IPV4_PREFIX_LEN 4
#define IPV6_PREFIX_LEN 16
#define HASH_NUM 4

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

#define BUFFER_SIZE 1024lu*1024lu*1024lu
#define INDEX_ENUM_LEN sizeof(IndexStore)
#define DATA_OFFSET BUFFER_SIZE*32
#define INDEX_OFFSET BUFFER_SIZE*48
#define INDEX_SIZE 131689368

BitMap* bitmap;
std::vector<PrefixBloomFilter*> prefixFilters;

struct IndexArrayMeta{
    u_int64_t index_array_meta[IndexType::TOTAL_INDEX*2];
};

std::vector<IndexArrayMeta> metas;
std::vector<IndexArrayMeta> index_metas;
std::vector<Record<u_int32_t>> srcipv4s;
std::vector<Record<u_int32_t>> dstipv4s;
std::vector<Record<u_int16_t>> srcports;
std::vector<Record<u_int16_t>> dstports;
std::vector<Record<IPv6Address>> srcipv6s;
std::vector<Record<IPv6Address>> dstipv6s;


// std::vector<u_int32_t> srcipv4s;
// std::vector<u_int32_t> dstipv4s;
// std::vector<IPv6Address> srcipv6s;
// std::vector<IPv6Address> dstipv6s;

u_int64_t data_disk_size = 1024lu*1024lu*1024lu*64lu;
u_int64_t data_block_size = 1024lu*1024lu*1024lu;
u_int64_t bitmap_backup_col_num = 8;
const size_t hash_num = HASH_NUM;

u_int64_t index_block_offset = 1024lu*1024lu*1024lu*1024lu;
char* buffer;


void fill(){
    const char* filename = "/dev/sdb";
    int read_fd = open(filename, O_RDWR);
    if (read_fd < 0) {
        perror("open for read");
        return;
    }

    buffer = new char[INDEX_SIZE];

    ssize_t ret = pread(read_fd, buffer, INDEX_SIZE, INDEX_OFFSET);
    if (ret < 0) perror("pread");

    u_int64_t count = 0;
    for(u_int64_t i = 0; i<INDEX_SIZE; i+=INDEX_ENUM_LEN){
        IndexStore* index_store = (IndexStore*)(buffer + i);

        Index* index = new Index();
        index->disk_block_id = index_store->position / data_block_size;
        index->position = index_store->position;
        index->ts = index->ts;
        index->meta.sourceAddress = std::string(index_store->sourceAddress,index_store->version);
        index->meta.destinationAddress = std::string(index_store->destinationAddress,index_store->version);
        index->meta.sourcePort = index_store->sourcePort;
        index->meta.destinationPort = index_store->destinationPort;

        if(index->disk_block_id >= prefixFilters.size()){
            PrefixBloomFilter* filter = new PrefixBloomFilter(bitmap,hash_num);
            filter->setWritingCol(index->disk_block_id);
            filter->setReadingCol(index->disk_block_id);
            prefixFilters.push_back(filter);

            IndexArrayMeta meta;

            if (index->disk_block_id != 0){
                metas[index->disk_block_id - 1].index_array_meta[IndexType::SRCIP * 2 + 1] = srcipv4s.size()*sizeof(Record<u_int32_t>); 
                metas[index->disk_block_id - 1].index_array_meta[IndexType::DSTIP * 2 + 1] = dstipv4s.size()*sizeof(Record<u_int32_t>); 
                metas[index->disk_block_id - 1].index_array_meta[IndexType::SRCPORT * 2 + 1] = srcports.size()*sizeof(Record<u_int16_t>); 
                metas[index->disk_block_id - 1].index_array_meta[IndexType::DSTPORT * 2 + 1] = dstports.size()*sizeof(Record<u_int16_t>); 
                metas[index->disk_block_id - 1].index_array_meta[IndexType::SRCIPv6 * 2 + 1] = srcipv6s.size()*sizeof(Record<IPv6Address>); 
                metas[index->disk_block_id - 1].index_array_meta[IndexType::DSTIPv6 * 2 + 1] = dstipv6s.size()*sizeof(Record<IPv6Address>); 

                std::sort(srcipv4s.begin() + metas[index->disk_block_id - 1].index_array_meta[IndexType::SRCIP * 2]/sizeof(Record<u_int32_t>),srcipv4s.end());
                std::sort(dstipv4s.begin() + metas[index->disk_block_id - 1].index_array_meta[IndexType::DSTIP * 2]/sizeof(Record<u_int32_t>),dstipv4s.end());
                std::sort(srcports.begin() + metas[index->disk_block_id - 1].index_array_meta[IndexType::SRCPORT * 2]/sizeof(Record<u_int16_t>),srcports.end());
                std::sort(dstports.begin() + metas[index->disk_block_id - 1].index_array_meta[IndexType::DSTPORT * 2]/sizeof(Record<u_int16_t>),dstports.end());
                std::sort(srcipv6s.begin() + metas[index->disk_block_id - 1].index_array_meta[IndexType::SRCIPv6 * 2]/sizeof(Record<IPv6Address>),srcipv6s.end());
                std::sort(dstipv6s.begin() + metas[index->disk_block_id - 1].index_array_meta[IndexType::DSTIPv6 * 2]/sizeof(Record<IPv6Address>),dstipv6s.end());
            }

            for(u_int64_t i=0;i<IndexType::TOTAL_INDEX;++i){
                meta.index_array_meta[i * 2] = index->disk_block_id == 0? 0 : metas[index->disk_block_id - 1].index_array_meta[i*2 + 1];
                meta.index_array_meta[i * 2 + 1] = 0;
            }

            metas.push_back(meta);

            
        }
        
        if(index_store->version == 4){
            prefixFilters[index->disk_block_id]->insertIPv4(*(u_int32_t*)(index->meta.sourceAddress.c_str()),IndexType::SRCIP);
            prefixFilters[index->disk_block_id]->insertIPv4(*(u_int32_t*)(index->meta.destinationAddress.c_str()),IndexType::DSTIP);

            Record<u_int32_t> srcip_record = {
                .key = *(u_int32_t*)(index->meta.sourceAddress.c_str()),
                .value = index->position,
            };
            srcipv4s.push_back(srcip_record);
            Record<u_int32_t> dstip_record = {
                .key = *(u_int32_t*)(index->meta.destinationAddress.c_str()),
                .value = index->position,
            };
            dstipv4s.push_back(dstip_record);
        }else{
            prefixFilters[index->disk_block_id]->insertIPv6(*(IPv6Address*)(index->meta.sourceAddress.c_str()),IndexType::SRCIPv6);
            prefixFilters[index->disk_block_id]->insertIPv6(*(IPv6Address*)(index->meta.destinationAddress.c_str()),IndexType::DSTIPv6);
            Record<IPv6Address> srcip_record = {
                .key = *(IPv6Address*)(index->meta.sourceAddress.c_str()),
                .value = index->position,
            };
            srcipv6s.push_back(srcip_record);
            Record<IPv6Address> dstip_record = {
                .key = *(IPv6Address*)(index->meta.destinationAddress.c_str()),
                .value = index->position,
            };
            dstipv6s.push_back(dstip_record);
        }

        prefixFilters[index->disk_block_id]->insertPort(index->meta.sourcePort,IndexType::SRCPORT);
        prefixFilters[index->disk_block_id]->insertPort(index->meta.destinationPort,IndexType::DSTPORT);

        Record<u_int16_t> srcport_record = {
            .key = index->meta.sourcePort,
            .value = index->position,
        };
        srcports.push_back(srcport_record);
        Record<u_int16_t> dstport_record = {
            .key = index->meta.destinationPort,
            .value = index->position,
        };
        dstports.push_back(dstport_record);

        count ++;
    }

    metas[metas.size() - 1].index_array_meta[IndexType::SRCIP * 2 + 1] = srcipv4s.size()*sizeof(Record<u_int32_t>); 
    metas[metas.size() - 1].index_array_meta[IndexType::DSTIP * 2 + 1] = dstipv4s.size()*sizeof(Record<u_int32_t>); 
    metas[metas.size() - 1].index_array_meta[IndexType::SRCPORT * 2 + 1] = srcports.size()*sizeof(Record<u_int16_t>); 
    metas[metas.size() - 1].index_array_meta[IndexType::DSTPORT * 2 + 1] = dstports.size()*sizeof(Record<u_int16_t>); 
    metas[metas.size() - 1].index_array_meta[IndexType::SRCIPv6 * 2 + 1] = srcipv6s.size()*sizeof(Record<IPv6Address>); 
    metas[metas.size() - 1].index_array_meta[IndexType::DSTIPv6 * 2 + 1] = dstipv6s.size()*sizeof(Record<IPv6Address>); 

    std::sort(srcipv4s.begin() + metas[metas.size() - 1].index_array_meta[IndexType::SRCIP * 2]/sizeof(Record<u_int32_t>),srcipv4s.end());
    std::sort(dstipv4s.begin() + metas[metas.size() - 1].index_array_meta[IndexType::DSTIP * 2]/sizeof(Record<u_int32_t>),dstipv4s.end());
    std::sort(srcports.begin() + metas[metas.size() - 1].index_array_meta[IndexType::SRCPORT * 2]/sizeof(Record<u_int16_t>),srcports.end());
    std::sort(dstports.begin() + metas[metas.size() - 1].index_array_meta[IndexType::DSTPORT * 2]/sizeof(Record<u_int16_t>),dstports.end());
    std::sort(srcipv6s.begin() + metas[metas.size() - 1].index_array_meta[IndexType::SRCIPv6 * 2]/sizeof(Record<IPv6Address>),srcipv6s.end());
    std::sort(dstipv6s.begin() + metas[metas.size() - 1].index_array_meta[IndexType::DSTIPv6 * 2]/sizeof(Record<IPv6Address>),dstipv6s.end());
    
    // printf("count: %lu, block: %lu\n",count, metas.size());
    u_int64_t total_len = 0;
    for(u_int64_t i = 0;i<IndexType::TOTAL_INDEX;++i){
        // for(auto x:metas){
        //     printf("type %lu with %lu-%lu\n",i,x.index_array_meta[i * 2],x.index_array_meta[i * 2 + 1]);
        // }
        total_len += metas.back().index_array_meta[i * 2 + 1];
    }
    // printf("total len: %lu\n",total_len);
    char* write_buffer = new char[total_len];
    std::vector<char*> indexes = std::vector<char*>({(char*)srcipv4s.data(),(char*)dstipv4s.data(),(char*)srcports.data(),(char*)dstports.data(),(char*)srcipv6s.data(),(char*)dstipv6s.data()});
    u_int64_t current_len = 0;
    for(auto meta: metas){
        IndexArrayMeta new_meta;
        for (u_int64_t j = 0;j<IndexType::TOTAL_INDEX;++j){
            // printf("%lu\n",current_len);
            memcpy(write_buffer + current_len,indexes[j]+meta.index_array_meta[j*2],meta.index_array_meta[j*2+1]-meta.index_array_meta[j*2]);
            new_meta.index_array_meta[j*2] = current_len;
            new_meta.index_array_meta[j*2+1] = current_len + meta.index_array_meta[j*2+1]-meta.index_array_meta[j*2];
            current_len = new_meta.index_array_meta[j*2+1];
            // printf("block %lu, type %lu, start %lu, end %lu, len %lu\n",index_metas.size(),j,new_meta.index_array_meta[j*2],new_meta.index_array_meta[j*2+1],new_meta.index_array_meta[j*2+1]-new_meta.index_array_meta[j*2]);
        }
        index_metas.push_back(new_meta);
    }

    ret = pwrite(read_fd, write_buffer, total_len, index_block_offset);
    if (ret < 0) perror("pwrite");

    // delete[] buffer;
    delete[] write_buffer;
}

void init(){
    bitmap = new BitMap((PORT_BIT_LEN + IPV4_BIT_LEN + IPV6_BIT_LEN) * 2, data_disk_size / data_block_size, bitmap_backup_col_num);
    prefixFilters = std::vector<PrefixBloomFilter*>();
    metas = std::vector<IndexArrayMeta>();
    index_metas = std::vector<IndexArrayMeta>();
}

static std::mt19937_64 rng(
    std::chrono::steady_clock::now().time_since_epoch().count()
);

u_int32_t generateRandomID(){
    static std::uniform_int_distribution<uint32_t> dist(0, (u_int32_t)INDEX_SIZE/sizeof(IndexStore));
    return dist(rng);
}

u_int16_t generateRandomPort() {
    static std::uniform_int_distribution<uint16_t> dist(0, 0xFFFFu);
    return dist(rng);
}

// 生成随机 IPv4 地址
u_int32_t generateRandomIPv4() {
    static std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);
    return dist(rng);
}

// 生成随机 IPv6 地址
IPv6Address generateRandomIPv6() {
    static std::uniform_int_distribution<uint64_t> dist64(0, 0xFFFFFFFFFFFFFFFFull);
    IPv6Address addr;
    addr.high = dist64(rng);
    addr.low  = dist64(rng);
    return addr;
}

bool checkIPv4(u_int32_t ip, IndexType type, u_int32_t prefix_len, u_int64_t block_id){
    return prefixFilters[block_id]->getIPv4(ip,type,prefix_len);
}
bool checkIPv6(IPv6Address ip, IndexType type, u_int32_t prefix_len, u_int64_t block_id){
    return prefixFilters[block_id]->getIPv6(ip,type,prefix_len);
}
bool checkPort(u_int16_t port, IndexType type, u_int64_t block_id){
    return prefixFilters[block_id]->getPort(port,type);
}

template <class KeyType>
void binarySearch(char* index, u_int64_t index_len, KeyType key, std::vector<u_int64_t>& ret){
    u_int64_t ele_len = sizeof(KeyType) + sizeof(u_int64_t);
    u_int64_t left = 0;
    u_int64_t right = index_len/ele_len;

    while (left < right) {
        u_int64_t mid = left + (right - left) / 2;

        KeyType key_mid = *(KeyType*)(index + mid * ele_len);
        if (key_mid < key) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    for(;left<index_len/ele_len;left++){
        KeyType key_now = *(KeyType*)(index + left * ele_len);
        if(key_now != key){
            break;
        }
        u_int64_t value = *(u_int64_t*)(index + left * ele_len + sizeof(KeyType));
        ret.push_back(value);
    }
}

template <class KeyType>
void binarySearchRange(char* index, u_int32_t index_len, KeyType startKey, KeyType endKey, std::vector<u_int64_t>& ret){
    u_int32_t ele_len = sizeof(KeyType) + sizeof(u_int64_t);
    u_int32_t left = 0;
    u_int32_t right = index_len/ele_len;

    while (left < right) {
        u_int32_t mid = left + (right - left) / 2;

        KeyType key_mid = *(KeyType*)(index + mid * ele_len);
        if (key_mid < startKey) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    for(;left<index_len/ele_len;left++){
        KeyType key_now = *(KeyType*)(index + left * ele_len);
        if(key_now >= endKey){
            break;
        }
        u_int64_t value = *(u_int64_t*)(index + left * ele_len + sizeof(KeyType));
        ret.push_back(value);
    }
}

std::vector<u_int64_t> getOffsetByIPv4(u_int32_t ip, IndexType type, u_int32_t prefix_len, u_int64_t block_id, u_int64_t fd){
    u_int64_t start = index_metas[block_id].index_array_meta[type*2];
    u_int64_t end = index_metas[block_id].index_array_meta[type*2+1];
    u_int64_t len = end - start;
    char* index_buffer = new char[len];

    ssize_t ret = pread(fd,index_buffer,len,index_block_offset + start);
    if (ret < 0) perror("pread");

    std::vector<u_int64_t> ret_vec = std::vector<u_int64_t>();
    
    if(prefix_len == sizeof(ip)){
        binarySearch(index_buffer,len,ip,ret_vec);
    }else{
        u_int32_t start_ip = ip & (((1u << (prefix_len * 8u)) - 1u) << (sizeof(ip) - prefix_len));
        u_int32_t end_ip = ip | ((1u << (sizeof(ip) - prefix_len)) - 1u);
        binarySearchRange(index_buffer,len,start_ip,end_ip,ret_vec);
    }
    std::sort(ret_vec.begin(),ret_vec.end());
    delete[] index_buffer;
    return ret_vec;
}

std::vector<u_int64_t> getOffsetByIPv6(IPv6Address ip, IndexType type, u_int32_t prefix_len, u_int64_t block_id, u_int64_t fd){
    u_int64_t start = index_metas[block_id].index_array_meta[type*2];
    u_int64_t end = index_metas[block_id].index_array_meta[type*2+1];
    u_int64_t len = end - start;
    char* index_buffer = new char[len];

    ssize_t ret = pread(fd,index_buffer,len,index_block_offset + start);
    if (ret < 0) perror("pread");

    std::vector<u_int64_t> ret_vec = std::vector<u_int64_t>();
    
    if(prefix_len == sizeof(ip)){
        binarySearch(index_buffer,len,ip,ret_vec);
    }else{
        IPv6Address start_ip = ip;
        IPv6Address end_ip = ip;
        if(prefix_len > 8){
            prefix_len -= 8;
            start_ip.low = start_ip.low & (((1lu << ((u_int64_t)prefix_len * 8lu)) - 1lu) << (sizeof(u_int64_t) - (u_int64_t)prefix_len));
            end_ip.low = end_ip.low | ((1u << (sizeof(u_int64_t) - prefix_len)) - 1u);
        }else{
            start_ip.high = start_ip.high & (((1lu << ((u_int64_t)prefix_len * 8lu)) - 1lu) << (sizeof(u_int64_t) - (u_int64_t)prefix_len));
            end_ip.high = end_ip.low | ((1u << (sizeof(u_int64_t) - prefix_len)) - 1u);
            start_ip.low = 0;
            end_ip.high = std::numeric_limits<uint64_t>::max();
        }
        binarySearchRange(index_buffer,len,start_ip,end_ip,ret_vec);
    }
    std::sort(ret_vec.begin(),ret_vec.end());
    delete[] index_buffer;
    return ret_vec;
}

std::vector<u_int64_t> getOffsetByPort(u_int16_t port, IndexType type, u_int64_t block_id, u_int64_t fd){
    u_int64_t start = index_metas[block_id].index_array_meta[type*2];
    u_int64_t end = index_metas[block_id].index_array_meta[type*2+1];
    u_int64_t len = end - start;
    char* index_buffer = new char[len];

    ssize_t ret = pread(fd,index_buffer,len,index_block_offset + start);
    if (ret < 0) perror("pread");

    std::vector<u_int64_t> ret_vec = std::vector<u_int64_t>();
    binarySearch(index_buffer,len,port,ret_vec);
    std::sort(ret_vec.begin(),ret_vec.end());

    delete[] index_buffer;
    return ret_vec;
}

std::vector<u_int64_t> join(std::vector<u_int64_t> v1, std::vector<u_int64_t> v2){
    std::vector<u_int64_t> ret = std::vector<u_int64_t>();
    auto ita = v1.begin();
    auto itb = v2.begin();
    while (ita != v1.end() && itb != v2.end()) {
        if (*ita < *itb) {
            ita++;
        } else if (*ita > *itb) {
            itb++;
        } else {
            ret.push_back(*ita);
            ita++;
            itb++;
        }
    }
    return ret;
}

std::pair<u_int64_t,u_int64_t> readPacket(std::vector<u_int64_t> offset_vec, int fd){
    u_int64_t packet_count = 0;
    u_int64_t byte_count = 0;
    char* data_buffer = new char[4096];
    for(auto offset:offset_vec){
        // printf("%lu\n",offset);
        while(true){
            ssize_t ret = pread(fd,data_buffer,4096,DATA_OFFSET + offset);
            if (ret < 0) perror("pread");
            array_list_header* header = (array_list_header*)data_buffer;
            packet_count ++;
            byte_count += header->caplen;
            // printf("diff %u\n",header->flow_next_diff);
            if(header->flow_next_diff == std::numeric_limits<uint32_t>::max()|| header->flow_next_diff == 0){
                break;
            }
            offset += (u_int64_t)header->flow_next_diff;
        }
    }
    delete[] data_buffer;
    return std::pair<u_int64_t,u_int64_t>(packet_count,byte_count);
}

void q1(){
    const char* filename = "/dev/sdb";
    int read_fd = open(filename, O_RDONLY);
    

    u_int64_t packet_count = 0;
    u_int64_t byte_count = 0;

    u_int64_t check_time = 0;
    u_int64_t index_time = 0;
    u_int64_t read_time = 0;

    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    for(u_int64_t i=0;i<QUERY_TIMES/4;++i){
        //printf("%lu\n",i);
        u_int32_t id = generateRandomID();
        // printf("id %u\n",id);
        IndexStore* index = (IndexStore*)buffer + id;
        if(index->version == 4){
            u_int32_t srcip = *(u_int32_t*)index->sourceAddress;
            u_int32_t dstip = *(u_int32_t*)index->destinationAddress;
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(srcip,IndexType::SRCIP,sizeof(srcip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(srcip,IndexType::SRCIP,sizeof(srcip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time+= std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            auto pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(dstip,IndexType::DSTIP,sizeof(dstip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(dstip,IndexType::DSTIP,sizeof(dstip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;
        }else{
            IPv6Address srcip = *(IPv6Address*)index->sourceAddress;
            IPv6Address dstip = *(IPv6Address*)index->destinationAddress;
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            auto pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;
        }

        u_int16_t srcport = index->sourcePort;
        u_int16_t dstport = index->destinationPort;
        std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
        for(u_int64_t j=0; j<index_metas.size();++j){
            t1 = std::chrono::high_resolution_clock::now();
            if(!checkPort(srcport,IndexType::SRCPORT,j)){
                continue;
            }
            t2 = std::chrono::high_resolution_clock::now();
            check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            // printf("j:%lu\n",j);
            t1 = std::chrono::high_resolution_clock::now();
            auto ret = getOffsetByPort(srcport,IndexType::SRCPORT,j,read_fd);
            if(ret.size()!=0){
                offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
            }
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }
        if(offset_vec.size()==0){
            printf("error, %lu\n",i);
        }
        t1 = std::chrono::high_resolution_clock::now();
        auto pa = readPacket(offset_vec,read_fd);
        t2 = std::chrono::high_resolution_clock::now();
        read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        packet_count += pa.first;
        byte_count += pa.second;

        offset_vec = std::vector<u_int64_t>();
        for(u_int64_t j=0; j<index_metas.size();++j){
            t1 = std::chrono::high_resolution_clock::now();
            if(!checkPort(dstport,IndexType::DSTPORT,j)){
                continue;
            }
            t2 = std::chrono::high_resolution_clock::now();
            check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            // printf("j:%lu\n",j);
            t1 = std::chrono::high_resolution_clock::now();
            auto ret = getOffsetByPort(dstport,IndexType::DSTPORT,j,read_fd);
            if(ret.size()!=0){
                offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
            }
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }
        if(offset_vec.size()==0){
            printf("error, %lu\n",i);
        }
        t1 = std::chrono::high_resolution_clock::now();
        pa = readPacket(offset_vec,read_fd);
        t2 = std::chrono::high_resolution_clock::now();
        read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        packet_count += pa.first;
        byte_count += pa.second;

    }
    printf("packet: %lu, byte: %lu\n",packet_count,byte_count);
    printf("check time %lu us, index time %lu us, read time %lu us\n",check_time,index_time,read_time);
}

void q2(){
    const char* filename = "/dev/sdb";
    int read_fd = open(filename, O_RDONLY);
    

    u_int64_t packet_count = 0;
    u_int64_t byte_count = 0;

    u_int64_t check_time = 0;
    u_int64_t index_time = 0;
    u_int64_t read_time = 0;

    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    for(u_int64_t i=0;i<QUERY_TIMES/20;++i){
        //printf("%lu\n",i);
        u_int32_t id = generateRandomID();
        // printf("id %u\n",id);
        IndexStore* index = (IndexStore*)buffer + id;
        if(index->version == 4){
            u_int32_t srcip = *(u_int32_t*)index->sourceAddress;
            u_int32_t dstip = *(u_int32_t*)index->destinationAddress;
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(srcip,IndexType::SRCIP,sizeof(srcip)-1,j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(srcip,IndexType::SRCIP,sizeof(srcip)-1,j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time+= std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            auto pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(dstip,IndexType::DSTIP,sizeof(dstip)-1,j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(dstip,IndexType::DSTIP,sizeof(dstip)-1,j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;
        }else{
            IPv6Address srcip = *(IPv6Address*)index->sourceAddress;
            IPv6Address dstip = *(IPv6Address*)index->destinationAddress;
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip)-2,j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip)-2,j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            auto pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip)-2,j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip)-2,j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;
        }

    }
    printf("packet: %lu, byte: %lu\n",packet_count,byte_count);
    printf("check time %lu us, index time %lu us, read time %lu us\n",check_time,index_time,read_time);
}

void q3(){
    const char* filename = "/dev/sdb";
    int read_fd = open(filename, O_RDONLY);
    

    u_int64_t packet_count = 0;
    u_int64_t byte_count = 0;

    u_int64_t check_time = 0;
    u_int64_t index_time = 0;
    u_int64_t read_time = 0;

    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    for(u_int64_t i=0;i<QUERY_TIMES;++i){
        //printf("%lu\n",i);
        u_int32_t id = generateRandomID();
        // printf("id %u\n",id);
        IndexStore* index = (IndexStore*)buffer + id;
        std::vector<u_int64_t> total_vec = std::vector<u_int64_t>();
        if(index->version == 4){
            u_int32_t srcip = *(u_int32_t*)index->sourceAddress;
            u_int32_t dstip = *(u_int32_t*)index->destinationAddress;
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(srcip,IndexType::SRCIP,sizeof(srcip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(srcip,IndexType::SRCIP,sizeof(srcip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time+= std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            total_vec = offset_vec;
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(dstip,IndexType::DSTIP,sizeof(dstip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(dstip,IndexType::DSTIP,sizeof(dstip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            total_vec = join(total_vec,offset_vec);
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        }else{
            IPv6Address srcip = *(IPv6Address*)index->sourceAddress;
            IPv6Address dstip = *(IPv6Address*)index->destinationAddress;
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            total_vec = offset_vec;
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                printf("error, %lu\n",i);
            }
            t1 = std::chrono::high_resolution_clock::now();
            total_vec = join(total_vec,offset_vec);
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }

        u_int16_t srcport = index->sourcePort;
        u_int16_t dstport = index->destinationPort;
        std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
        for(u_int64_t j=0; j<index_metas.size();++j){
            t1 = std::chrono::high_resolution_clock::now();
            if(!checkPort(srcport,IndexType::SRCPORT,j)){
                continue;
            }
            t2 = std::chrono::high_resolution_clock::now();
            check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            // printf("j:%lu\n",j);
            t1 = std::chrono::high_resolution_clock::now();
            auto ret = getOffsetByPort(srcport,IndexType::SRCPORT,j,read_fd);
            if(ret.size()!=0){
                offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
            }
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }
        if(offset_vec.size()==0){
            printf("error, %lu\n",i);
        }
        t1 = std::chrono::high_resolution_clock::now();
        total_vec = join(total_vec,offset_vec);
        t2 = std::chrono::high_resolution_clock::now();
        index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        offset_vec = std::vector<u_int64_t>();
        for(u_int64_t j=0; j<index_metas.size();++j){
            t1 = std::chrono::high_resolution_clock::now();
            if(!checkPort(dstport,IndexType::DSTPORT,j)){
                continue;
            }
            t2 = std::chrono::high_resolution_clock::now();
            check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            // printf("j:%lu\n",j);
            t1 = std::chrono::high_resolution_clock::now();
            auto ret = getOffsetByPort(dstport,IndexType::DSTPORT,j,read_fd);
            if(ret.size()!=0){
                offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
            }
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }
        if(offset_vec.size()==0){
            printf("error, %lu\n",i);
        }
        t1 = std::chrono::high_resolution_clock::now();
        total_vec = join(total_vec,offset_vec);
        t2 = std::chrono::high_resolution_clock::now();
        index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        t1 = std::chrono::high_resolution_clock::now();
        auto pa = readPacket(offset_vec,read_fd);
        t2 = std::chrono::high_resolution_clock::now();
        read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        packet_count += pa.first;
        byte_count += pa.second;

    }
    printf("packet: %lu, byte: %lu\n",packet_count,byte_count);
    printf("check time %lu us, index time %lu us, read time %lu us\n",check_time,index_time,read_time);
}

void q4(){
    const char* filename = "/dev/sdb";
    int read_fd = open(filename, O_RDONLY);
    

    u_int64_t packet_count = 0;
    u_int64_t byte_count = 0;
    u_int64_t get_count = 0;

    u_int64_t check_time = 0;
    u_int64_t index_time = 0;
    u_int64_t read_time = 0;

    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    for(u_int64_t i=0;i<QUERY_TIMES*10/4;++i){
        //printf("%lu\n",i);
        u_int32_t id = generateRandomID();
        // printf("id %u\n",id);
        IndexStore* index = (IndexStore*)buffer + id;
        if(index->version == 4){
            u_int32_t srcip = generateRandomIPv4();
            u_int32_t dstip = generateRandomIPv4();
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(srcip,IndexType::SRCIP,sizeof(srcip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(srcip,IndexType::SRCIP,sizeof(srcip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time+= std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                get_count ++;
            }
            t1 = std::chrono::high_resolution_clock::now();
            auto pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(dstip,IndexType::DSTIP,sizeof(dstip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(dstip,IndexType::DSTIP,sizeof(dstip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                get_count ++;
            }
            t1 = std::chrono::high_resolution_clock::now();
            pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;
        }else{
            IPv6Address srcip = generateRandomIPv6();
            IPv6Address dstip = generateRandomIPv6();
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                get_count ++;
            }
            t1 = std::chrono::high_resolution_clock::now();
            auto pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                get_count ++;
            }
            t1 = std::chrono::high_resolution_clock::now();
            pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;
        }

        u_int16_t srcport = generateRandomPort();
        u_int16_t dstport = generateRandomPort();
        std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
        for(u_int64_t j=0; j<index_metas.size();++j){
            t1 = std::chrono::high_resolution_clock::now();
            if(!checkPort(srcport,IndexType::SRCPORT,j)){
                continue;
            }
            t2 = std::chrono::high_resolution_clock::now();
            check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            // printf("j:%lu\n",j);
            t1 = std::chrono::high_resolution_clock::now();
            auto ret = getOffsetByPort(srcport,IndexType::SRCPORT,j,read_fd);
            if(ret.size()!=0){
                offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
            }
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }
        if(offset_vec.size()==0){
            get_count ++;
        }
        t1 = std::chrono::high_resolution_clock::now();
        auto pa = readPacket(offset_vec,read_fd);
        t2 = std::chrono::high_resolution_clock::now();
        read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        packet_count += pa.first;
        byte_count += pa.second;

        offset_vec = std::vector<u_int64_t>();
        for(u_int64_t j=0; j<index_metas.size();++j){
            t1 = std::chrono::high_resolution_clock::now();
            if(!checkPort(dstport,IndexType::DSTPORT,j)){
                continue;
            }
            t2 = std::chrono::high_resolution_clock::now();
            check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            // printf("j:%lu\n",j);
            t1 = std::chrono::high_resolution_clock::now();
            auto ret = getOffsetByPort(dstport,IndexType::DSTPORT,j,read_fd);
            if(ret.size()!=0){
                offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
            }
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }
        if(offset_vec.size()==0){
            get_count ++;
        }
        t1 = std::chrono::high_resolution_clock::now();
        pa = readPacket(offset_vec,read_fd);
        t2 = std::chrono::high_resolution_clock::now();
        read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        packet_count += pa.first;
        byte_count += pa.second;

    }
    printf("packet: %lu, byte: %lu, get: %lu\n",packet_count,byte_count,get_count);
    printf("check time %lu us, index time %lu us, read time %lu us\n",check_time,index_time,read_time);
}

void q5(){
    const char* filename = "/dev/sdb";
    int read_fd = open(filename, O_RDONLY);
    

    u_int64_t packet_count = 0;
    u_int64_t byte_count = 0;
    u_int64_t get_count = 0;

    u_int64_t check_time = 0;
    u_int64_t index_time = 0;
    u_int64_t read_time = 0;

    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    for(u_int64_t i=0;i<QUERY_TIMES/2;++i){
        //printf("%lu\n",i);
        u_int32_t id = generateRandomID();
        // printf("id %u\n",id);
        IndexStore* index = (IndexStore*)buffer + id;
        if(index->version == 4){
            u_int32_t srcip = generateRandomIPv4();
            u_int32_t dstip = generateRandomIPv4();
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(srcip,IndexType::SRCIP,sizeof(srcip)-1,j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(srcip,IndexType::SRCIP,sizeof(srcip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time+= std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                get_count++;
            }
            t1 = std::chrono::high_resolution_clock::now();
            auto pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(dstip,IndexType::DSTIP,sizeof(dstip)-1,j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(dstip,IndexType::DSTIP,sizeof(dstip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                get_count ++;
            }
            t1 = std::chrono::high_resolution_clock::now();
            pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;
        }else{
            IPv6Address srcip = generateRandomIPv6();
            IPv6Address dstip = generateRandomIPv6();
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip)-2,j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip)-2,j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                get_count++;
            }
            t1 = std::chrono::high_resolution_clock::now();
            auto pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip)-2,j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip)-2,j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            if(offset_vec.size()==0){
                get_count++;
            }
            t1 = std::chrono::high_resolution_clock::now();
            pa = readPacket(offset_vec,read_fd);
            t2 = std::chrono::high_resolution_clock::now();
            read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            packet_count += pa.first;
            byte_count += pa.second;
        }

    }
    printf("packet: %lu, byte: %lu, get: %lu\n",packet_count,byte_count,get_count);
    printf("check time %lu us, index time %lu us, read time %lu us\n",check_time,index_time,read_time);
}

void q6(){
    const char* filename = "/dev/sdb";
    int read_fd = open(filename, O_RDONLY);
    

    u_int64_t packet_count = 0;
    u_int64_t byte_count = 0;

    u_int64_t check_time = 0;
    u_int64_t index_time = 0;
    u_int64_t read_time = 0;

    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();

    for(u_int64_t i=0;i<QUERY_TIMES*10;++i){
        //printf("%lu\n",i);
        u_int32_t id = generateRandomID();
        // printf("id %u\n",id);
        IndexStore* index = (IndexStore*)buffer + id;
        std::vector<u_int64_t> total_vec = std::vector<u_int64_t>();
        if(index->version == 4){
            u_int32_t srcip = generateRandomIPv4();
            u_int32_t dstip = generateRandomIPv4();
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(srcip,IndexType::SRCIP,sizeof(srcip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(srcip,IndexType::SRCIP,sizeof(srcip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time+= std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            
            t1 = std::chrono::high_resolution_clock::now();
            total_vec = offset_vec;
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv4(dstip,IndexType::DSTIP,sizeof(dstip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv4(dstip,IndexType::DSTIP,sizeof(dstip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            t1 = std::chrono::high_resolution_clock::now();
            total_vec = join(total_vec,offset_vec);
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        }else{
            IPv6Address srcip = generateRandomIPv6();
            IPv6Address dstip = generateRandomIPv6();
            std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                // printf("j:%lu\n",j);
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(srcip,IndexType::SRCIPv6,sizeof(srcip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            t1 = std::chrono::high_resolution_clock::now();
            total_vec = offset_vec;
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

            offset_vec = std::vector<u_int64_t>();
            for(u_int64_t j=0; j<index_metas.size();++j){
                t1 = std::chrono::high_resolution_clock::now();
                if(!checkIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip),j)){
                    continue;
                }
                t2 = std::chrono::high_resolution_clock::now();
                // printf("j:%lu\n",j);
                check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
                t1 = std::chrono::high_resolution_clock::now();
                auto ret = getOffsetByIPv6(dstip,IndexType::DSTIPv6,sizeof(dstip),j,read_fd);
                if(ret.size()!=0){
                    offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
                }
                t2 = std::chrono::high_resolution_clock::now();
                index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            }
            t1 = std::chrono::high_resolution_clock::now();
            total_vec = join(total_vec,offset_vec);
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }

        u_int16_t srcport = generateRandomPort();
        u_int16_t dstport = generateRandomPort();
        std::vector<u_int64_t> offset_vec = std::vector<u_int64_t>();
        for(u_int64_t j=0; j<index_metas.size();++j){
            t1 = std::chrono::high_resolution_clock::now();
            if(!checkPort(srcport,IndexType::SRCPORT,j)){
                continue;
            }
            t2 = std::chrono::high_resolution_clock::now();
            check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            // printf("j:%lu\n",j);
            t1 = std::chrono::high_resolution_clock::now();
            auto ret = getOffsetByPort(srcport,IndexType::SRCPORT,j,read_fd);
            if(ret.size()!=0){
                offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
            }
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }
        t1 = std::chrono::high_resolution_clock::now();
        total_vec = join(total_vec,offset_vec);
        t2 = std::chrono::high_resolution_clock::now();
        index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        offset_vec = std::vector<u_int64_t>();
        for(u_int64_t j=0; j<index_metas.size();++j){
            t1 = std::chrono::high_resolution_clock::now();
            if(!checkPort(dstport,IndexType::DSTPORT,j)){
                continue;
            }
            t2 = std::chrono::high_resolution_clock::now();
            check_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            // printf("j:%lu\n",j);
            t1 = std::chrono::high_resolution_clock::now();
            auto ret = getOffsetByPort(dstport,IndexType::DSTPORT,j,read_fd);
            if(ret.size()!=0){
                offset_vec.insert(offset_vec.end(),ret.begin(),ret.end());
            }
            t2 = std::chrono::high_resolution_clock::now();
            index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        }

        t1 = std::chrono::high_resolution_clock::now();
        total_vec = join(total_vec,offset_vec);
        t2 = std::chrono::high_resolution_clock::now();
        index_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

        if(total_vec.size()!=0){
            printf("rare!\n");
        }else{
            continue;
        }

        t1 = std::chrono::high_resolution_clock::now();
        auto pa = readPacket(offset_vec,read_fd);
        t2 = std::chrono::high_resolution_clock::now();
        read_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        packet_count += pa.first;
        byte_count += pa.second;

    }
    printf("packet: %lu, byte: %lu\n",packet_count,byte_count);
    printf("check time %lu us, index time %lu us, read time %lu us\n",check_time,index_time,read_time);
}

int main(){
    init();
    fill();
    printf("query begin\n");
    q6();
    return 0;
}