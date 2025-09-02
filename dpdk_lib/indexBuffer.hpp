#ifndef INDEX_BUFFER_HPP_
#define INDEX_BUFFER_HPP_

#include <cmath>
#include "skipList.hpp"
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
    SkipList skiplists[IndexType::TOTAL_INDEX];
    // char skiplistHeads[SKIPLISTNODE_HEAD_LEN];
    u_int64_t disk_block_id;
    void init(BitMap* bitmap, size_t k, u_int64_t disk_block_num){
        this->bloomFilterMeta.init(bitmap, k);
        this->bloomFilterMeta.setWritingCol(disk_block_num);
        this->disk_block_id = disk_block_num;

        u_int64_t offset = 0;
        
        this->skiplists[IndexType::SRCIP].init(sizeof(u_int32_t) * 8, sizeof(u_int32_t), sizeof(u_int64_t));
        // SkipListNode<u_int32_t,u_int64_t>* srcIPNode = (SkipListNode<u_int32_t,u_int64_t>*)(this->skiplistHeads + offset);
        // srcIPNode->init(0, 0, sizeof(u_int32_t) * 8);
        // offset += IPV4_SKIPLISTNODE_HEAD_LEN;
        // this->skiplists[IndexType::SRCIP].addHead(srcIPNode);

        this->skiplists[IndexType::DSTIP].init(sizeof(u_int32_t) * 8, sizeof(u_int32_t), sizeof(u_int64_t));
        // SkipListNode<u_int32_t,u_int64_t>* dstIPNode = (SkipListNode<u_int32_t,u_int64_t>*)(this->skiplistHeads + offset);
        // dstIPNode->init(0, 0, sizeof(u_int32_t) * 8);
        // offset += IPV4_SKIPLISTNODE_HEAD_LEN;
        // this->skiplists[IndexType::DSTIP].addHead(dstIPNode);

        this->skiplists[IndexType::SRCPORT].init(sizeof(u_int16_t) * 8, sizeof(u_int16_t), sizeof(u_int64_t));
        // SkipListNode<u_int16_t,u_int64_t>* srcPortNode = (SkipListNode<u_int16_t,u_int64_t>*)(this->skiplistHeads + offset);
        // srcPortNode->init(0, 0, sizeof(u_int16_t) * 8);
        // offset += PORT_SKIPLISTNODE_HEAD_LEN;
        // this->skiplists[IndexType::SRCPORT].addHead(srcPortNode);

        this->skiplists[IndexType::DSTPORT].init(sizeof(u_int16_t) * 8, sizeof(u_int16_t), sizeof(u_int64_t));
        // SkipListNode<u_int16_t,u_int64_t>* dstPortNode = (SkipListNode<u_int16_t,u_int64_t>*)(this->skiplistHeads + offset);
        // dstPortNode->init(0, 0, sizeof(u_int16_t) * 8);
        // offset += PORT_SKIPLISTNODE_HEAD_LEN;
        // this->skiplists[IndexType::DSTPORT].addHead(dstPortNode);

        this->skiplists[IndexType::SRCIPv6].init(sizeof(IPv6Address) * 8, sizeof(IPv6Address), sizeof(u_int64_t));
        // SkipListNode<IPv6Address,u_int64_t>* srcIPv6Node = (SkipListNode<IPv6Address,u_int64_t>*)(this->skiplistHeads + offset);
        // srcIPv6Node->init(IPv6Address{0, 0}, 0, sizeof(IPv6Address) * 8);
        // offset += IPV6_SKIPLISTNODE_HEAD_LEN;
        // this->skiplists[IndexType::SRCIPv6].addHead(srcIPv6Node);

        this->skiplists[IndexType::DSTIPv6].init(sizeof(IPv6Address) * 8, sizeof(IPv6Address), sizeof(u_int64_t));
        // SkipListNode<IPv6Address,u_int64_t>* dstIPv6Node = (SkipListNode<IPv6Address,u_int64_t>*)(this->skiplistHeads + offset);
        // dstIPv6Node->init(IPv6Address{0, 0}, 0, sizeof(IPv6Address) * 8);
        // offset += IPV6_SKIPLISTNODE_HEAD_LEN;
        // this->skiplists[IndexType::DSTIPv6].addHead(dstIPv6Node);

        // this->skiplists[IndexType::QUARTURPLEIPv4].init(sizeof(QuarTurpleIPv4) * 8, sizeof(QuarTurpleIPv4), sizeof(u_int64_t));
        // SkipListNode<QuarTurpleIPv4,u_int64_t>* quarTurpleIPv4Node = (SkipListNode<QuarTurpleIPv4,u_int64_t>*)(this->skiplistHeads + offset);
        // quarTurpleIPv4Node->init(QuarTurpleIPv4{0, 0, 0, 0}, 0, sizeof(QuarTurpleIPv4) * 8);
        // offset += QUARTURPLEIPV4_SKIPLISTNODE_HEAD_LEN;
        // this->skiplists[IndexType::QUARTURPLEIPv4].addHead(quarTurpleIPv4Node);

        // this->skiplists[IndexType::QUARTURPLEIPv6].init(sizeof(QuarTurpleIPv6) * 8, sizeof(QuarTurpleIPv6), sizeof(u_int64_t));
        // SkipListNode<QuarTurpleIPv6,u_int64_t>* quarTurpleIPv6Node = (SkipListNode<QuarTurpleIPv6,u_int64_t>*)(this->skiplistHeads + offset);
        // quarTurpleIPv6Node->init(QuarTurpleIPv6{0, 0, IPv6Address{0, 0}, IPv6Address{0, 0}}, 0, sizeof(QuarTurpleIPv6) * 8);
        // offset += QUARTURPLEIPV6_SKIPLISTNODE_HEAD_LEN;
        // this->skiplists[IndexType::QUARTURPLEIPv6].addHead(quarTurpleIPv6Node);
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
public:
    IndexBuffer(u_int64_t total_block_num, u_int64_t disk_block_num, BitMap* bitmap, size_t k):
        total_block_num(total_block_num), disk_block_num(disk_block_num){
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
    }
    ~IndexBuffer(){
        munmap(this->metas, this->size);
    }
    u_int64_t addCheckThread(){
        u_int64_t id = this->index_check_ids.size();
        if(id >= this->total_block_num){
            printf("Data block buffer error: too many check threads!\n");
            return std::numeric_limits<uint64_t>::max();
        }
        this->index_check_ids.push_back(id);
        return id;
    }
    bool insert(FlowMetadata& meta, u_int64_t position, u_int64_t disk_block_id, u_int64_t ts){
        u_int64_t buffer_meta_id = disk_block_id % this->total_block_num;
        if (this->metas[buffer_meta_id].disk_block_id != disk_block_id){
            return false;
        }

        // printf("Insert type %u\n",type);

        // printf("insert ip\n");

        if (meta.sourceAddress.size()==sizeof(u_int32_t) && meta.destinationAddress.size() == sizeof(u_int32_t)){
            // printf("a\n");
            u_int32_t* srcip = (u_int32_t*)(meta.sourceAddress.c_str());
            if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv4(*srcip, IndexType::SRCIP)){
                printf("Insert bloom filter fail at IPv4.\n");
                return false;
            }
            // printf("b\n");
            if (!this->metas[buffer_meta_id].skiplists[IndexType::SRCIP].insert(meta.sourceAddress,position)){
                printf("Insert skiplist fail at IPv4.\n");
                return false;
            }
            // printf("c\n");
            u_int32_t* dstip = (u_int32_t*)(meta.destinationAddress.c_str());
            if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv4(*dstip, IndexType::DSTIP)){
                printf("Insert bloom filter fail at srcIPv4.\n");
                return false;
            }
            // printf("d\n");
            if (!this->metas[buffer_meta_id].skiplists[IndexType::DSTIP].insert(meta.destinationAddress,position)){
                printf("Insert skiplist fail at srcIPv4.\n");
                return false;
            }
            // printf("e\n");
            // QuarTurpleIPv4 turple = {
            //     .dstport = meta.destinationPort,
            //     .srcport = meta.sourcePort,
            //     .dstip = *dstip,
            //     .srcip = *srcip,
            // };
            // // printf("f\n");
            // auto key = std::string((char*)(&turple),sizeof(turple));
            // if (!this->metas[buffer_meta_id].skiplists[IndexType::QUARTURPLEIPv4].insert(key,position)){
            //     printf("Insert skiplist fail at IPv4turple.\n");
            //     return false;
            // }
            // printf("g\n");
        }else if(meta.sourceAddress.size()==sizeof(IPv6Address) && meta.destinationAddress.size() == sizeof(IPv6Address)){
            IPv6Address* srcip = (IPv6Address*)(meta.sourceAddress.c_str());
            if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv6(*srcip, IndexType::SRCIPv6)){
                printf("Insert bloom filter fail at srcIPv6.\n");
                return false;
            }
            if (!this->metas[buffer_meta_id].skiplists[IndexType::SRCIPv6].insert(meta.sourceAddress,position)){
                printf("Insert skiplist fail at srcIPv6.\n");
                return false;
            }
            IPv6Address* dstip = (IPv6Address*)(meta.destinationAddress.c_str());
            if (!this->metas[buffer_meta_id].bloomFilterMeta.insertIPv6(*dstip, IndexType::DSTIPv6)){
                printf("Insert bloom filter fail at dstIPv6.\n");
                return false;
            }
            if (!this->metas[buffer_meta_id].skiplists[IndexType::DSTIPv6].insert(meta.destinationAddress,position)){
                printf("Insert skiplist fail at dstIPv6.\n");
                return false;
            }
            // QuarTurpleIPv6 turple = {
            //     .dstport = meta.destinationPort,
            //     .srcport = meta.sourcePort,
            //     .dstip = *dstip,
            //     .srcip = *srcip,
            // };
            // auto key = std::string((char*)(&turple),sizeof(turple));
            // if (!this->metas[buffer_meta_id].skiplists[IndexType::QUARTURPLEIPv6].insert(key,position)){
            //     printf("Insert skiplist fail at IPv4turple.\n");
            //     return false;
            // }
        }

        // printf("insert port\n");

        if (!this->metas[buffer_meta_id].bloomFilterMeta.insertPort(meta.sourcePort, IndexType::SRCPORT)){
            printf("Insert bloom filter fail at sport.\n");
            return false;
        }
        auto keysport = std::string((char*)(&meta.sourcePort),sizeof(meta.sourcePort));
        if (!this->metas[buffer_meta_id].skiplists[IndexType::SRCPORT].insert(keysport,position)){
            printf("Insert skiplist fail at sport.\n");
            return false;
        }

        if (!this->metas[buffer_meta_id].bloomFilterMeta.insertPort(meta.destinationPort, IndexType::DSTPORT)){
            printf("Insert bloom filter fail at dport.\n");
            return false;
        }

        auto keydport = std::string((char*)(&meta.destinationPort),sizeof(meta.destinationPort));
        if (!this->metas[buffer_meta_id].skiplists[IndexType::DSTPORT].insert(keydport,position)){
            printf("Insert skiplist fail at dport.\n");
            return false;
        }

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
    u_int64_t getCheckDishID(u_int64_t thread_id){
        if (thread_id >= this->index_check_ids.size()) {
            printf("Index buffer error: thread_id %lu out of range!\n", thread_id);
            return std::numeric_limits<u_int64_t>::max();
        }
        u_int64_t block_check_id = this->index_check_ids[thread_id];
        return this->metas[block_check_id].disk_block_id;
    }
    u_int64_t checkIndexCount(u_int64_t thread_id, IndexType type){
        if (thread_id >= this->index_check_ids.size()) {
            printf("Index buffer error: thread_id %lu out of range!\n", thread_id);
            return std::numeric_limits<u_int64_t>::max();
        }
        u_int64_t block_check_id = this->index_check_ids[thread_id];
        return this->metas[block_check_id].skiplists[type].getNodeNum();
    }
    IndexBufferMeta* getIndexBufferMeta(u_int64_t thread_id){
        if (thread_id >= this->index_check_ids.size()) {
            printf("Index buffer error: thread_id %lu out of range!\n", thread_id);
            return nullptr;
        }
        u_int64_t block_check_id = this->index_check_ids[thread_id];
        return &this->metas[block_check_id];
    }
    void updateIndexBufferMeta(u_int64_t thread_id, u_int64_t bitmap_col){
        if (thread_id >= this->index_check_ids.size()) {
            printf("Index buffer error: thread_id %lu out of range!\n", thread_id);
            return;
        }
        u_int64_t block_check_id = this->index_check_ids[thread_id];
        for (u_int32_t type = IndexType::SRCIP; type < IndexType::TOTAL_INDEX; ++type){
            this->metas[block_check_id].skiplists[type].clear();
        }
        this->metas[block_check_id].bloomFilterMeta.setWritingCol(bitmap_col);
        this->metas[block_check_id].disk_block_id = (this->metas[block_check_id].disk_block_id + this->total_block_num) % this->disk_block_num;
        
        // TODO: Check position competition?
        this->index_check_ids[thread_id] = (this->index_check_ids[thread_id] + this->index_check_ids.size()) % this->total_block_num;

    }
};

// struct SkipListMeta{
//     u_int32_t maxLvl;
//     u_int32_t keyLen;
//     u_int32_t valueLen;
// };

// class IndexBuffer{
//     const u_int32_t cacheCount;
//     // const u_int32_t maxLvl;
//     // const u_int32_t keyLen;
//     // const u_int32_t valueLen;
//     const SkipListMeta meta;
//     const u_int64_t maxNode;

//     // SkipList** caches;
//     SkipList** caches;
//     bool* cacheFlag; // true for read
//     u_int64_t* start_times;

// public:
//     IndexBuffer(u_int32_t _cacheCount, SkipListMeta meta, u_int32_t _maxNode):
//     cacheCount(_cacheCount),maxNode(_maxNode),meta(meta){
//         this->caches = new SkipList*[_cacheCount];
//         for(u_int32_t i=0; i<_cacheCount; ++i){
//             this->caches[i] = new SkipList(meta.maxLvl,meta.keyLen,meta.valueLen);
//         }
//         this->cacheFlag = new bool[_cacheCount]();
//         this->start_times = new u_int64_t[_cacheCount];
//         for(u_int32_t i=0; i<_cacheCount; ++i){
//             this->start_times[i] = std::numeric_limits<uint64_t>::max();
//         }
//     }
//     ~IndexBuffer(){
//         for(u_int32_t i=0; i<this->cacheCount; ++i){
//             delete this->caches[i];
//         }
//         delete[] this->start_times;
//         delete[] this->cacheFlag;
//         delete[] this->caches;
//     }
//     bool insert(std::string& key, u_int64_t value, u_int32_t id, u_int64_t ts, u_int32_t start){
//         if(this->cacheFlag[id]){
//             printf("Index Buffer log: %u is full.\n",id);
//             return false;
//         }
//         // if(this->caches[id]->insert(keys,value,this->maxNode, start)){
//         //     this->start_times[id] = std::min(this->start_times[id],ts);
//         //     return true;
//         // }
//         if(this->caches[id]->insert(key,value,this->maxNode)){
//             this->start_times[id] = std::min(this->start_times[id],ts);
//             return true;
//         }
//         printf("Index Buffer log: %u is full.\n",id);
//         this->cacheFlag[id] = true;
//         return false;
//     }
//     std::pair<SkipList*,u_int64_t> getCache(u_int32_t id){
//         if(!this->cacheFlag[id]){
//             return {nullptr,0};
//         }
//         SkipList* ret = this->caches[id];
//         u_int64_t ts = this->start_times[id];
//         this->caches[id] = new SkipList(this->meta.maxLvl,this->meta.keyLen,this->meta.valueLen);
//         this->cacheFlag[id] = false;
//         return {ret,ts};
//     }
//     std::pair<SkipList*,u_int64_t> directGetCache(u_int32_t id){
//         SkipList* ret = this->caches[id];
//         u_int64_t ts = this->start_times[id];
//         this->caches[id] = new SkipList(this->meta.maxLvl,this->meta.keyLen,this->meta.valueLen);
//         this->cacheFlag[id] = false;
//         return {ret,ts};
//     }
//     u_int32_t getCacheCount()const{
//         return this->cacheCount;
//     }
// };

#endif