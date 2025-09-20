#include "../dpdk_lib/prefixBloomFilter.hpp"
#include "../dpdk_lib/bloomFilter.hpp"

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

#define QUERY_TIMES 10000
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
PrefixBloomFilter* prefixFilter;
BloomFilter* srcipv4filter;
BloomFilter* dstipv4filter;
BloomFilter* srcipv6filter;
BloomFilter* dstipv6filter;

// std::vector<u_int32_t> srcipv4s;
// std::vector<u_int32_t> dstipv4s;
// std::vector<IPv6Address> srcipv6s;
// std::vector<IPv6Address> dstipv6s;

inline uint8_t get_byte(const uint32_t& addr, int i) {
    return (addr >> (24 - 8 * i)) & 0xFF;
}

inline uint8_t get_byte(const IPv6Address& addr, int i) {
    if (i < 8) {
        return (addr.high >> (56 - 8 * i)) & 0xFF;
    } else {
        int j = i - 8;
        return (addr.low >> (56 - 8 * j)) & 0xFF;
    }
}

// Trie 节点
struct TrieNode {
    bool is_end = false;
    TrieNode* child[256] = {nullptr}; // 每个字节 0-255
};

template <typename AddrType, int NBytes>
class IPTrie {
public:
    IPTrie() : root(new TrieNode) {}
    ~IPTrie() { free_node(root); }

    // 插入完整 IP
    void insert(const AddrType& addr) {
        TrieNode* node = root;
        for (int i = 0; i < NBytes; i++) {
            uint8_t b = get_byte(addr, i);
            if (!node->child[b]) node->child[b] = new TrieNode();
            node = node->child[b];
        }
        // node->is_ip = true;
    }

    // 查询某个完整 IP 是否存在
    bool contains(const AddrType& addr) const {
        TrieNode* node = root;
        for (int i = 0; i < NBytes; i++) {
            uint8_t b = get_byte(addr, i);
            if (!node->child[b]) return false;
            node = node->child[b];
        }
        return true;
    }

    // 查询某个前缀下是否存在至少一个 IP
    bool contains_prefix(const AddrType& addr, int depth) const {
        // if (prefix_len % 8 != 0) {
        //     throw std::runtime_error("prefix_len 必须是 8 的倍数");
        // }
        // int depth = prefix_len / 8;
        TrieNode* node = root;
        for (int i = 0; i < depth; i++) {
            uint8_t b = get_byte(addr, i);
            if (!node->child[b]) return false;
            node = node->child[b];
        }
        return true;
    }

private:
    TrieNode* root;

    void free_node(TrieNode* node) {
        if (!node) return;
        for (int i = 0; i < 256; i++) {
            free_node(node->child[i]);
        }
        delete node;
    }

    // 判断某个子树下是否有完整 IP
    bool has_ip(const TrieNode* node) const {
        if (!node) return false;
        // if (node->is_ip) return true;
        for (int i = 0; i < 256; i++) {
            if (has_ip(node->child[i])) return true;
        }
        return false;
    }
};

IPTrie<u_int32_t,sizeof(u_int32_t)> srcipv4tree;
IPTrie<u_int32_t,sizeof(u_int32_t)> dstipv4tree;
IPTrie<IPv6Address,sizeof(IPv6Address)> srcipv6tree;
IPTrie<IPv6Address,sizeof(IPv6Address)> dstipv6tree;

u_int64_t data_disk_size = 1024lu*1024lu*1024lu*64lu;
u_int64_t data_block_size = 1024lu*1024lu*1024lu;
u_int64_t bitmap_backup_col_num = 8;
const size_t hash_num = HASH_NUM;


void fill(){
    const char* filename = "/dev/sdb";
    int read_fd = open(filename, O_RDONLY);
    if (read_fd < 0) {
        perror("open for read");
        return;
    }

    char* buffer = new char[INDEX_SIZE];

    ssize_t ret = pread(read_fd, buffer, INDEX_SIZE, INDEX_OFFSET);
    if (ret < 0) perror("pread");

    u_int64_t count = 4;
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

        if(index->disk_block_id > 0){
            break;
        }
        
        if(index_store->version == 4){
            prefixFilter->insertIPv4(*(u_int32_t*)(index->meta.sourceAddress.c_str()),IndexType::SRCIP);
            prefixFilter->insertIPv4(*(u_int32_t*)(index->meta.destinationAddress.c_str()),IndexType::DSTIP);
            if(!(prefixFilter->getIPv4(*(u_int32_t*)(index->meta.sourceAddress.c_str()),IndexType::SRCIP)&&prefixFilter->getIPv4(*(u_int32_t*)(index->meta.destinationAddress.c_str()),IndexType::DSTIP))){
                printf("error\n");
            }
            srcipv4filter->insert(index->meta.sourceAddress);
            dstipv4filter->insert(index->meta.destinationAddress);
            if(!(srcipv4filter->contains(index->meta.sourceAddress)&&dstipv4filter->contains(index->meta.destinationAddress))){
                printf("error 2\n");
            }

            srcipv4tree.insert(*(u_int32_t*)(index->meta.sourceAddress.c_str()));
            dstipv4tree.insert(*(u_int32_t*)(index->meta.destinationAddress.c_str()));
            if(!(srcipv4tree.contains(*(u_int32_t*)(index->meta.sourceAddress.c_str()))&&dstipv4tree.contains(*(u_int32_t*)(index->meta.destinationAddress.c_str())))){
                printf("error 3 1\n");
            }

            for(u_int32_t i=1;i<index->meta.sourceAddress.size()-1;++i){
                srcipv4filter->insert(index->meta.sourceAddress.substr(i,index->meta.sourceAddress.size()-i));
                dstipv4filter->insert(index->meta.destinationAddress.substr(i,index->meta.destinationAddress.size()-i));
                if(!(srcipv4filter->contains(index->meta.sourceAddress.substr(i,index->meta.sourceAddress.size()-i))&&dstipv4filter->contains(index->meta.destinationAddress.substr(i,index->meta.destinationAddress.size()-i)))){
                    printf("error 2\n");
                }
                if(!(prefixFilter->getIPv4(*(u_int32_t*)(index->meta.sourceAddress.c_str()),IndexType::SRCIP,i)&&prefixFilter->getIPv4(*(u_int32_t*)(index->meta.destinationAddress.c_str()),IndexType::DSTIP,i))){
                    printf("error\n");
                }
                if(!(srcipv4tree.contains_prefix(*(u_int32_t*)(index->meta.sourceAddress.c_str()),i)&&dstipv4tree.contains_prefix(*(u_int32_t*)(index->meta.destinationAddress.c_str()),i))){
                    printf("error 3 2\n");
                }
            }
            
        }else{
            prefixFilter->insertIPv6(*(IPv6Address*)(index->meta.sourceAddress.c_str()),IndexType::SRCIPv6);
            prefixFilter->insertIPv6(*(IPv6Address*)(index->meta.destinationAddress.c_str()),IndexType::DSTIPv6);
            if(!(prefixFilter->getIPv6(*(IPv6Address*)(index->meta.sourceAddress.c_str()),IndexType::SRCIPv6)&&prefixFilter->getIPv6(*(IPv6Address*)(index->meta.destinationAddress.c_str()),IndexType::DSTIPv6))){
                printf("error\n");
            }
            // printf("a\n");

            srcipv6filter->insert(index->meta.sourceAddress);
            dstipv6filter->insert(index->meta.destinationAddress);
            // printf("b\n");
            if(!(srcipv6filter->contains(index->meta.sourceAddress)&&dstipv6filter->contains(index->meta.destinationAddress))){
                printf("error 2\n");
            }

            srcipv6tree.insert(*(IPv6Address*)(index->meta.sourceAddress.c_str()));
            dstipv6tree.insert(*(IPv6Address*)(index->meta.destinationAddress.c_str()));
            if(!(srcipv6tree.contains(*(IPv6Address*)(index->meta.sourceAddress.c_str()))&&dstipv6tree.contains(*(IPv6Address*)(index->meta.destinationAddress.c_str())))){
                printf("error 3 1\n");
            }
            // printf("c\n");
            for(u_int32_t i=1;i<index->meta.sourceAddress.size()-1;++i){
                srcipv6filter->insert(index->meta.sourceAddress.substr(i,index->meta.sourceAddress.size()-i));
                dstipv6filter->insert(index->meta.destinationAddress.substr(i,index->meta.destinationAddress.size()-i));
                if(!(srcipv6filter->contains(index->meta.sourceAddress.substr(i,index->meta.sourceAddress.size()-i))&&dstipv6filter->contains(index->meta.destinationAddress.substr(i,index->meta.destinationAddress.size()-i)))){
                    printf("error 2\n");
                }
                if(!(prefixFilter->getIPv6(*(IPv6Address*)(index->meta.sourceAddress.c_str()),IndexType::SRCIPv6,i)&&prefixFilter->getIPv6(*(IPv6Address*)(index->meta.destinationAddress.c_str()),IndexType::DSTIPv6,i))){
                    printf("error\n");
                }
                if(!(srcipv6tree.contains_prefix(*(IPv6Address*)(index->meta.sourceAddress.c_str()),i)&&dstipv6tree.contains_prefix(*(IPv6Address*)(index->meta.destinationAddress.c_str()),i))){
                    printf("error 3 2\n");
                }
            }
        }
        // if (count % 10000 == 0){
        //     printf("%lu %lu\n",count,index->position);
        // }
        count ++;
        // index->rx_id = count % THREAD_NUM;
        // (*(indexRings))[count % THREAD_NUM]->put(index);
    }
    printf("count: %lu\n",count);
}

void init(){
    bitmap = new BitMap((PORT_BIT_LEN + IPV4_BIT_LEN + IPV6_BIT_LEN) * 2, data_disk_size / data_block_size, bitmap_backup_col_num);
    prefixFilter = new PrefixBloomFilter(bitmap,hash_num);
    prefixFilter->setReadingCol(0);
    prefixFilter->setWritingCol(0);

    srcipv4filter = new BloomFilter(IPV4_BIT_LEN/8,hash_num);
    dstipv4filter = new BloomFilter(IPV4_BIT_LEN/8,hash_num);
    srcipv6filter = new BloomFilter(IPV6_BIT_LEN/8,hash_num);
    dstipv6filter = new BloomFilter(IPV6_BIT_LEN/8,hash_num);
}

static std::mt19937_64 rng(
    std::chrono::steady_clock::now().time_since_epoch().count()
);

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

std::vector<u_int64_t> srcipv4_p;
std::vector<u_int64_t> dstipv4_p;
std::vector<u_int64_t> srcipv6_p;
std::vector<u_int64_t> dstipv6_p;
std::vector<u_int64_t> srcipv4_b;
std::vector<u_int64_t> dstipv4_b;
std::vector<u_int64_t> srcipv6_b;
std::vector<u_int64_t> dstipv6_b;


void query_ipv4(u_int32_t prefix_len){
    u_int64_t fp_count_src_pf = 0;
    u_int64_t tp_count_src_pf = 0;
    u_int64_t tf_count_src_pf = 0;
    u_int64_t fp_count_dst_pf = 0;
    u_int64_t tp_count_dst_pf = 0;
    u_int64_t tf_count_dst_pf = 0;
    u_int64_t fp_count_src_bf = 0;
    u_int64_t tp_count_src_bf = 0;
    u_int64_t tf_count_src_bf = 0;
    u_int64_t fp_count_dst_bf = 0;
    u_int64_t tp_count_dst_bf = 0;
    u_int64_t tf_count_dst_bf = 0;
    for(u_int64_t i; i<QUERY_TIMES;++i){
        u_int32_t random_ipv4 = generateRandomIPv4();
        if(prefix_len == 4){
            if(prefixFilter->getIPv4(random_ipv4,SRCIP)){
                if(srcipv4tree.contains(random_ipv4)){
                    tp_count_src_pf ++;
                }else{
                    fp_count_src_pf ++;
                }
            }else{
                tf_count_src_pf ++;
            }
            if(prefixFilter->getIPv4(random_ipv4,DSTIP)){
                if(dstipv4tree.contains(random_ipv4)){
                    tp_count_dst_pf ++;
                }else{
                    fp_count_dst_pf ++;
                }
            }else{
                tf_count_dst_pf ++;
            }

            std::string ipv4_str = std::string((char*)&random_ipv4,sizeof(random_ipv4));
            if(srcipv4filter->contains(ipv4_str)){
                if(srcipv4tree.contains(random_ipv4)){
                    tp_count_src_bf ++;
                }else{
                    fp_count_src_bf ++;
                }
            }else{
                tf_count_src_bf ++;
            }
            if(dstipv4filter->contains(ipv4_str)){
                if(dstipv4tree.contains(random_ipv4)){
                    tp_count_dst_bf ++;
                }else{
                    fp_count_dst_bf ++;
                }
            }else{
                tf_count_dst_bf ++;
            }
            continue;
        }

        if(prefixFilter->getIPv4(random_ipv4,SRCIP,prefix_len)){
            if(srcipv4tree.contains_prefix(random_ipv4,prefix_len)){
                tp_count_src_pf ++;
            }else{
                fp_count_src_pf ++;
            }
        }else{
            // if(srcipv4tree.contains_prefix(random_ipv4,prefix_len)){
            //     printf("wrong!\n");
            // }
            tf_count_src_pf ++;
        }
        if(prefixFilter->getIPv4(random_ipv4,DSTIP,prefix_len)){
            if(dstipv4tree.contains_prefix(random_ipv4,prefix_len)){
                tp_count_dst_pf ++;
            }else{
                fp_count_dst_pf ++;
            }
        }else{
            tf_count_dst_pf ++;
        }

        std::string ipv4_str = std::string((char*)&random_ipv4,sizeof(random_ipv4));
        if(srcipv4filter->contains(ipv4_str.substr(sizeof(random_ipv4)-prefix_len,prefix_len))){
            if(srcipv4tree.contains_prefix(random_ipv4,prefix_len)){
                tp_count_src_bf ++;
            }else{
                fp_count_src_bf ++;
            }
        }else{
            tf_count_src_bf ++;
        }
        if(dstipv4filter->contains(ipv4_str.substr(sizeof(random_ipv4)-prefix_len,prefix_len))){
            if(dstipv4tree.contains_prefix(random_ipv4,prefix_len)){
                tp_count_dst_bf ++;
            }else{
                fp_count_dst_bf ++;
            }
        }else{
            tf_count_dst_bf ++;
        }
    }
    printf("prefix filter src: tp %lu, fp %lu, tf %lu\n",tp_count_src_pf, fp_count_src_pf, tf_count_src_pf);
    printf("prefix filter dst: tp %lu, fp %lu, tf %lu\n",tp_count_dst_pf, fp_count_dst_pf, tf_count_dst_pf);
    printf("bloom filter src: tp %lu, fp %lu, tf %lu\n",tp_count_src_bf, fp_count_src_bf, tf_count_src_bf);
    printf("bloom filter dst: tp %lu, fp %lu, tf %lu\n",tp_count_dst_bf, fp_count_dst_bf, tf_count_dst_bf);
    srcipv4_p.push_back(fp_count_src_pf);
    srcipv4_b.push_back(fp_count_src_bf);
    dstipv4_p.push_back(fp_count_dst_pf);
    dstipv4_b.push_back(fp_count_dst_bf);
}

void query_ipv6(u_int32_t prefix_len){
    u_int64_t fp_count_src_pf = 0;
    u_int64_t tp_count_src_pf = 0;
    u_int64_t tf_count_src_pf = 0;
    u_int64_t fp_count_dst_pf = 0;
    u_int64_t tp_count_dst_pf = 0;
    u_int64_t tf_count_dst_pf = 0;
    u_int64_t fp_count_src_bf = 0;
    u_int64_t tp_count_src_bf = 0;
    u_int64_t tf_count_src_bf = 0;
    u_int64_t fp_count_dst_bf = 0;
    u_int64_t tp_count_dst_bf = 0;
    u_int64_t tf_count_dst_bf = 0;
    for(u_int64_t i; i<QUERY_TIMES;++i){
        IPv6Address random_ipv6 = generateRandomIPv6();
        if(prefix_len == 16){
            if(prefixFilter->getIPv6(random_ipv6,SRCIPv6)){
                if(srcipv6tree.contains(random_ipv6)){
                    tp_count_src_pf ++;
                }else{
                    fp_count_src_pf ++;
                }
            }else{
                tf_count_src_pf ++;
            }
            if(prefixFilter->getIPv6(random_ipv6,DSTIPv6)){
                if(dstipv6tree.contains(random_ipv6)){
                    tp_count_dst_pf ++;
                }else{
                    fp_count_dst_pf ++;
                }
            }else{
                tf_count_dst_pf ++;
            }

            std::string ipv6_str = std::string((char*)&random_ipv6,sizeof(random_ipv6));
            if(srcipv6filter->contains(ipv6_str)){
                if(srcipv6tree.contains(random_ipv6)){
                    tp_count_src_bf ++;
                }else{
                    fp_count_src_bf ++;
                }
            }else{
                tf_count_src_bf ++;
            }
            if(dstipv6filter->contains(ipv6_str)){
                if(dstipv6tree.contains(random_ipv6)){
                    tp_count_dst_bf ++;
                }else{
                    fp_count_dst_bf ++;
                }
            }else{
                tf_count_dst_bf ++;
            }
            continue;
        }

        if(prefixFilter->getIPv6(random_ipv6,SRCIPv6,prefix_len)){
            if(srcipv6tree.contains_prefix(random_ipv6,prefix_len)){
                tp_count_src_pf ++;
            }else{
                fp_count_src_pf ++;
            }
        }else{
            tf_count_src_pf ++;
        }
        if(prefixFilter->getIPv6(random_ipv6,DSTIPv6,prefix_len)){
            if(dstipv6tree.contains_prefix(random_ipv6,prefix_len)){
                tp_count_dst_pf ++;
            }else{
                fp_count_dst_pf ++;
            }
        }else{
            tf_count_dst_pf ++;
        }

        std::string ipv6_str = std::string((char*)&random_ipv6,sizeof(random_ipv6));
        if(srcipv6filter->contains(ipv6_str.substr(sizeof(random_ipv6)-prefix_len,prefix_len))){
            if(srcipv6tree.contains_prefix(random_ipv6,prefix_len)){
                tp_count_src_bf ++;
            }else{
                fp_count_src_bf ++;
            }
        }else{
            tf_count_src_bf ++;
        }
        if(dstipv6filter->contains(ipv6_str.substr(sizeof(random_ipv6)-prefix_len,prefix_len))){
            if(dstipv6tree.contains_prefix(random_ipv6,prefix_len)){
                tp_count_dst_bf ++;
            }else{
                fp_count_dst_bf ++;
            }
        }else{
            tf_count_dst_bf ++;
        }
    }
    printf("prefix filter srcv6: tp %lu, fp %lu, tf %lu\n",tp_count_src_pf, fp_count_src_pf, tf_count_src_pf);
    printf("prefix filter dstv6: tp %lu, fp %lu, tf %lu\n",tp_count_dst_pf, fp_count_dst_pf, tf_count_dst_pf);
    printf("bloom filter srcv6: tp %lu, fp %lu, tf %lu\n",tp_count_src_bf, fp_count_src_bf, tf_count_src_bf);
    printf("bloom filter dstv6: tp %lu, fp %lu, tf %lu\n",tp_count_dst_bf, fp_count_dst_bf, tf_count_dst_bf);
    srcipv6_p.push_back(fp_count_src_pf);
    srcipv6_b.push_back(fp_count_src_bf);
    dstipv6_p.push_back(fp_count_dst_pf);
    dstipv6_b.push_back(fp_count_dst_bf);
}

int main(){
    init();
    fill();
    printf("query begin\n");
    for(u_int32_t i=1;i<=4;++i){
        query_ipv4(i);
        printf("\n");
    }
    for(u_int32_t i=1;i<=16;++i){
        query_ipv6(i);
        printf("%u\n",i);
    }

    printf("srcipv4: prefix %f, %lu; bloom: %f, %lu\n",std::accumulate(srcipv4_p.begin(), srcipv4_p.end(), 0.0),srcipv4_p[3],std::accumulate(srcipv4_b.begin(), srcipv4_b.end(), 0.0),srcipv4_b[3]);
    printf("dstipv4: prefix %f, %lu; bloom: %f, %lu\n",std::accumulate(dstipv4_p.begin(), dstipv4_p.end(), 0.0),dstipv4_p[3],std::accumulate(dstipv4_b.begin(), dstipv4_b.end(), 0.0),dstipv4_b[3]);
    printf("srcipv6: prefix %f, %lu; bloom: %f, %lu\n",std::accumulate(srcipv6_p.begin(), srcipv6_p.end(), 0.0),srcipv6_p[15],std::accumulate(srcipv6_b.begin(), srcipv6_b.end(), 0.0),srcipv6_b[15]);
    printf("dstipv6: prefix %f, %lu; bloom: %f, %lu\n",std::accumulate(dstipv6_p.begin(), dstipv6_p.end(), 0.0),dstipv6_p[15],std::accumulate(dstipv6_b.begin(), dstipv6_b.end(), 0.0),dstipv6_b[15]);

    return 0;
}