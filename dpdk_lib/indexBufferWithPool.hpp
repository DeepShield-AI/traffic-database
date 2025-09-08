#ifndef INDEX_BUFFER_HPP_
#define INDEX_BUFFER_HPP_

#include <cmath>
#include "indexMemoryPool.hpp"
#include "prefixBloomFilter.hpp"
#include "util.hpp"

#define IPV4_SKIPLISTNODE_HEAD_LEN (sizeof(SkipListNode<u_int32_t, u_int64_t>) + sizeof(void*) * (sizeof(u_int32_t) * 8 - 1))
#define PORT_SKIPLISTNODE_HEAD_LEN (sizeof(SkipListNode<u_int16_t, u_int64_t>) + sizeof(void*) * (sizeof(u_int16_t) * 8 - 1))
#define IPV6_SKIPLISTNODE_HEAD_LEN (sizeof(SkipListNode<IPv6Address, u_int64_t>) + sizeof(void*) * (sizeof(IPv6Address) * 8 - 1))
#define QUARTURPLEIPV4_SKIPLISTNODE_HEAD_LEN (sizeof(SkipListNode<QuarTurpleIPv4, u_int64_t>) + sizeof(void*) * (sizeof(QuarTurpleIPv4) * 8 - 1))
#define QUARTURPLEIPV6_SKIPLISTNODE_HEAD_LEN (sizeof(SkipListNode<QuarTurpleIPv6, u_int64_t>) + sizeof(void*) * (sizeof(QuarTurpleIPv6) * 8 - 1))
#define SKIPLISTNODE_HEAD_LEN (IPV4_SKIPLISTNODE_HEAD_LEN * 2 + PORT_SKIPLISTNODE_HEAD_LEN * 2 + IPV6_SKIPLISTNODE_HEAD_LEN * 2 + QUARTURPLEIPV4_SKIPLISTNODE_HEAD_LEN + QUARTURPLEIPV6_SKIPLISTNODE_HEAD_LEN)

struct IndexBufferMeta{
    PrefixBloomFilter bloomFilterMeta;
    // char skiplistHeads[SKIPLISTNODE_HEAD_LEN];
    u_int64_t disk_block_id;
    std::atomic_uint64_t index_count;
    void init(BitMap* bitmap, size_t k, u_int64_t disk_block_num){
        this->bloomFilterMeta.init(bitmap, k);
        this->bloomFilterMeta.setWritingCol(disk_block_num);
        this->disk_block_id = disk_block_num;
        this->index_count.store(0);
    }
};

class IndexBuffer {
private:
    const u_int64_t total_block_num;
    const u_int64_t disk_block_num;
    u_int64_t size;

    IndexBufferMeta* metas;
    // std::vector<u_int64_t> index_write_ids;
    std::vector<u_int64_t> index_check_ids;
    std::vector<IndexMemoryPool*>* memory_pools;
public:
    IndexBuffer(u_int64_t total_block_num, u_int64_t disk_block_num, BitMap* bitmap, size_t k, std::vector<IndexMemoryPool*>* memory_pools):
        total_block_num(total_block_num), disk_block_num(disk_block_num), memory_pools(memory_pools) {
        if (disk_block_num % total_block_num != 0) {
            printf("Index buffer error: disk_block_num must be a multiple of total_block_num!\n");
            throw std::runtime_error("disk_block_num must be a multiple of total_block_num");
        }
        this->size = this->total_block_num * sizeof(IndexBufferMeta);
        this->metas = (IndexBufferMeta*)mmap(nullptr, this->size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (this->metas == MAP_FAILED){
            printf("Index buffer error: mmap failed for metas!\n");
            throw std::runtime_error("memory manager mmap failed");
        }
        // printf("meta address %lu\n", (u_int64_t)this->metas);
        for (u_int64_t i = 0; i < this->total_block_num; ++i){
            this->metas[i].init(bitmap, k, i);
        }
        this->index_check_ids = std::vector<u_int64_t>();
        // this->memory_pools = std::vector<IndexMemoryPool*>();
    }
    ~IndexBuffer(){
        munmap(this->metas, this->size);
    }
    // void addMemoryPool(IndexMemoryPool* pool){
    //     this->memory_pools.push_back(pool);
    // }
    u_int64_t addCheckThread(){
        u_int64_t id = this->index_check_ids.size();
        if(id >= this->total_block_num){
            printf("Data block buffer error: too many check threads!\n");
            return std::numeric_limits<uint64_t>::max();
        }
        this->index_check_ids.push_back(id);
        return id;
    }
    bool insert(FlowMetadata& meta, u_int64_t position, u_int64_t disk_block_id, u_int64_t ts, u_int64_t rx_id){
        u_int64_t buffer_meta_id = disk_block_id % this->total_block_num;
        if (this->metas[buffer_meta_id].disk_block_id != disk_block_id){
            return false;
        }

        // printf("Insert type %u\n",type);

        // printf("insert ip\n");

        if (meta.sourceAddress.size()==sizeof(u_int32_t) && meta.destinationAddress.size() == sizeof(u_int32_t)){
            // printf("a\n");
            u_int32_t* srcip = (u_int32_t*)(meta.sourceAddress.c_str());
            // printf("srcip:%u\n",*srcip);
            if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv4(*srcip, IndexType::SRCIP)){
                printf("Insert bloom filter fail at IPv4.\n");
                return false;
            }
            // printf("b\n");
            u_int32_t* dstip = (u_int32_t*)(meta.destinationAddress.c_str());
            if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv4(*dstip, IndexType::DSTIP)){
                printf("Insert bloom filter fail at srcIPv4.\n");
                return false;
            }
        }else if(meta.sourceAddress.size()==sizeof(IPv6Address) && meta.destinationAddress.size() == sizeof(IPv6Address)){
            IPv6Address* srcip = (IPv6Address*)(meta.sourceAddress.c_str());
            if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv6(*srcip, IndexType::SRCIPv6)){
                printf("Insert bloom filter fail at srcIPv6.\n");
                return false;
            }
            IPv6Address* dstip = (IPv6Address*)(meta.destinationAddress.c_str());
            if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv6(*dstip, IndexType::DSTIPv6)){
                printf("Insert bloom filter fail at dstIPv6.\n");
                return false;
            }
        }

        if (!this->metas[buffer_meta_id].bloomFilterMeta.insertPort(meta.sourcePort, IndexType::SRCPORT)){
            printf("Insert bloom filter fail at sport.\n");
            return false;
        }
        
        if (!this->metas[buffer_meta_id].bloomFilterMeta.insertPort(meta.destinationPort, IndexType::DSTPORT)){
            printf("Insert bloom filter fail at dport.\n");
            return false;
        }

        if(!(*(this->memory_pools))[rx_id]->insert(meta,position, disk_block_id)){
            printf("Insert memory pool fail.\n");
            return false;
        }

        // if(this->metas[buffer_meta_id].index_count < 10){
        //     printf("Insert index meta: sport %u, dport %u, pos %lu, disk id %lu\n",
        //         meta.sourcePort, meta.destinationPort, position, disk_block_id);
        // }

        this->metas[buffer_meta_id].index_count ++;

        return true;

        // if (type == IndexType::SRCIP || type == IndexType::DSTIP){
        //     SkipListNode<u_int32_t,u_int64_t>* ipNode = (SkipListNode<u_int32_t,u_int64_t>*)node;
        //     u_int32_t ip = ipNode->key;
        //     // printf("node value: %lu\n",ipNode->value);
        //     if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv4(ip, type)){
        //         printf("Insert bloom filter fail at IPv4.\n");
        //         return false;
        //     }
        // }else if (type == IndexType::SRCPORT || type == IndexType::DSTPORT){
        //     SkipListNode<u_int16_t,u_int64_t>* portNode = (SkipListNode<u_int16_t,u_int64_t>*)node;
        //     u_int16_t port = portNode->key;
        //     // printf("node value: %lu\n",portNode->value);
        //     if (!this->metas[buffer_meta_id].bloomFilterMeta.insertPort(port, type)){
        //         printf("Insert bloom filter fail at port.\n");
        //         return false;
        //     }
        // }else if (type == IndexType::SRCIPv6 || type == IndexType::DSTIPv6){
        //     SkipListNode<IPv6Address,u_int64_t>* ipNode = (SkipListNode<IPv6Address,u_int64_t>*)node;
        //     IPv6Address ip = ipNode->key;
        //     // printf("node value: %lu\n",ipNode->value);
        //     if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv6(ip, type)){
        //         printf("Insert bloom filter fail at IPv6.\n");
        //         return false;
        //     }
        // }

        // printf("bitmap finish\n");
        // return this->metas[buffer_meta_id].skiplists[type].insert(node);
    }
    // bool insert(void* node, u_int64_t disk_block_id, IndexType type, u_int64_t ts){
    //     u_int64_t buffer_meta_id = disk_block_id % this->total_block_num;
    //     if (this->metas[buffer_meta_id].disk_block_id != disk_block_id){
    //         return false;
    //     }

    //     // printf("Insert type %u\n",type);

    //     if (type == IndexType::SRCIP || type == IndexType::DSTIP){
    //         SkipListNode<u_int32_t,u_int64_t>* ipNode = (SkipListNode<u_int32_t,u_int64_t>*)node;
    //         u_int32_t ip = ipNode->key;
    //         // printf("node value: %lu\n",ipNode->value);
    //         if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv4(ip, type)){
    //             printf("Insert bloom filter fail at IPv4.\n");
    //             return false;
    //         }
    //     }else if (type == IndexType::SRCPORT || type == IndexType::DSTPORT){
    //         SkipListNode<u_int16_t,u_int64_t>* portNode = (SkipListNode<u_int16_t,u_int64_t>*)node;
    //         u_int16_t port = portNode->key;
    //         // printf("node value: %lu\n",portNode->value);
    //         if (!this->metas[buffer_meta_id].bloomFilterMeta.insertPort(port, type)){
    //             printf("Insert bloom filter fail at port.\n");
    //             return false;
    //         }
    //     }else if (type == IndexType::SRCIPv6 || type == IndexType::DSTIPv6){
    //         SkipListNode<IPv6Address,u_int64_t>* ipNode = (SkipListNode<IPv6Address,u_int64_t>*)node;
    //         IPv6Address ip = ipNode->key;
    //         // printf("node value: %lu\n",ipNode->value);
    //         if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv6(ip, type)){
    //             printf("Insert bloom filter fail at IPv6.\n");
    //             return false;
    //         }
    //     }

    //     // printf("bitmap finish\n");
    //     return this->metas[buffer_meta_id].skiplists[type].insert(node);
    // }
    // get insert node number of skiplist
    u_int64_t checkIndexCount(u_int64_t thread_id){
        if (thread_id >= this->index_check_ids.size()) {
            printf("Index buffer error: thread_id %lu out of range!\n", thread_id);
            return std::numeric_limits<u_int64_t>::max();
        }
        u_int64_t block_check_id = this->index_check_ids[thread_id];
        return this->metas[block_check_id].index_count.load();
    }
    u_int64_t getCheckDishID(u_int64_t thread_id){
        if (thread_id >= this->index_check_ids.size()) {
            printf("Index buffer error: thread_id %lu out of range!\n", thread_id);
            return std::numeric_limits<u_int64_t>::max();
        }
        u_int64_t block_check_id = this->index_check_ids[thread_id];
        return this->metas[block_check_id].disk_block_id;
    }
    IndexBufferMeta* getIndexBufferMeta(u_int64_t thread_id){
        if (thread_id >= this->index_check_ids.size()) {
            printf("Index buffer error: thread_id %lu out of range!\n", thread_id);
            return nullptr;
        }
        u_int64_t block_check_id = this->index_check_ids[thread_id];
        return &this->metas[block_check_id];
    }
    // void persistIndex(u_int64_t thread_id, u_int64_t write_thread_id, IndexBlockBuffer* buffer, u_int64_t disk_pos){
    //     if (thread_id >= this->index_check_ids.size()) {
    //         printf("Index buffer error: thread_id %lu out of range!\n", thread_id);
    //         return;
    //     }
    //     u_int64_t block_check_id = this->index_check_ids[thread_id];
    //     for (auto pool : this->memory_pools){
    //         for (u_int32_t type = IndexType::SRCIP; type < IndexType::TOTAL_INDEX; ++type){
    //             pool->writeToBuffer(type, buffer, block_check_id, pool->getIndexLen(type,block_check_id),write_thread_id,disk_pos);
    //         }
    //     }
    // }
    void updateIndexBufferMeta(u_int64_t thread_id, u_int64_t bitmap_col){
        if (thread_id >= this->index_check_ids.size()) {
            printf("Index buffer error: thread_id %lu out of range!\n", thread_id);
            return;
        }
        u_int64_t block_check_id = this->index_check_ids[thread_id];
        // for (u_int32_t type = IndexType::SRCIP; type < IndexType::TOTAL_INDEX; ++type){
        //     this->metas[block_check_id].skiplists[type].clear();
        // }
        this->metas[block_check_id].bloomFilterMeta.setWritingCol(bitmap_col);
        for(auto pool: *(this->memory_pools)){
            pool->recycle(this->metas[block_check_id].disk_block_id);
        }
        this->metas[block_check_id].index_count.store(0);
        this->metas[block_check_id].disk_block_id = (this->metas[block_check_id].disk_block_id + this->total_block_num) % this->disk_block_num;
        // TODO: Check position competition?
        this->index_check_ids[thread_id] = (this->index_check_ids[thread_id] + this->index_check_ids.size()) % this->total_block_num;

    }
};

#endif