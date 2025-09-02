#ifndef INDEX_MEMORY_POOL_HPP_
#define INDEX_MEMORY_POOL_HPP_

#include "memoryPool.hpp"
#include "util.hpp"

class IndexMemoryPool{
private:
    char* buffer;
    u_int64_t capacity;
    u_int64_t list_len;
    MemoryPool* pool[IndexType::TOTAL_INDEX];
public:
    IndexMemoryPool(u_int64_t capacity,u_int64_t list_len):
        capacity(capacity),list_len(list_len){
        this->buffer = (char*)mmap(nullptr, this->capacity, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (this->buffer == MAP_FAILED){
            printf("Index Memory pool error: mmap failed for blocks!\n");
            throw std::runtime_error("memory manager mmap failed");
        }
        u_int64_t offset = 0;
        u_int64_t pool_capacity = this->capacity / IndexType::TOTAL_INDEX;
        for(int i=0;i<IndexType::TOTAL_INDEX;i++){
            u_int64_t unit_len = 0;
            if(i == IndexType::SRCIP || i == IndexType::DSTIP){
                unit_len = sizeof(u_int32_t) + sizeof(u_int64_t);
            }else if(i == IndexType::SRCPORT || i == IndexType::DSTPORT){
                unit_len = sizeof(u_int16_t) + sizeof(u_int64_t);
            }else if(i == IndexType::SRCIPv6 || i == IndexType::DSTIPv6){
                unit_len = sizeof(IPv6Address) + sizeof(u_int64_t);
            }
            this->pool[i] = new MemoryPool(this->buffer + offset,,this->list_len);
        }
    }
    ~IndexMemoryPool(){
        for(int i=0;i<IndexType::TOTAL_INDEX;i++){
            delete this->pool[i];
        }
        munmap(buffer,this->capacity);
    }
    bool insert(FlowMetadata& meta, u_int64_t position,  u_int64_t disk_block_id){
        if(meta.sourceAddress.size()==sizeof(u_int32_t) && meta.destinationAddress.size() == sizeof(u_int32_t)){
            u_int32_t* srcip = (u_int32_t*)(meta.sourceAddress.c_str());
            u_int32_t* dstip = (u_int32_t*)(meta.destinationAddress.c_str());
            char* p = this->pool[IndexType::SRCIP]->allocate(sizeof(u_int32_t)+sizeof(u_int64_t),disk_block_id);
            // if(p == nullptr || !this->pool[IndexType::SRCIP]->insert((char*)srcip,sizeof(u_int32_t),disk_block_id)){
            //     return false;
            // }
            memcpy(p,srcip,sizeof(u_int32_t));
            memcpy(p+sizeof(u_int32_t),(char*)&position,sizeof(u_int64_t));
            p = this->pool[IndexType::DSTIP]->allocate(sizeof(u_int32_t)+sizeof(u_int64_t),disk_block_id);
            memcpy(p,dstip,sizeof(u_int32_t));
            memcpy(p+sizeof(u_int32_t),(char*)&position,sizeof(u_int64_t));
        }else if(meta.sourceAddress.size()==sizeof(IPv6Address) && meta.destinationAddress.size() == sizeof(IPv6Address)){
            IPv6Address* srcip = (IPv6Address*)(meta.sourceAddress.c_str());
            IPv6Address* dstip = (IPv6Address*)(meta.destinationAddress.c_str());
            char* p = this->pool[IndexType::SRCIPv6]->allocate(sizeof(IPv6Address)+sizeof(u_int64_t),disk_block_id);
            memcpy(p,srcip,sizeof(IPv6Address));
            memcpy(p+sizeof(IPv6Address),(char*)&position,sizeof(u_int64_t));
            p = this->pool[IndexType::DSTIPv6]->allocate(sizeof(IPv6Address)+sizeof(u_int64_t),disk_block_id);
            memcpy(p,dstip,sizeof(IPv6Address));
            memcpy(p+sizeof(IPv6Address),(char*)&position,sizeof(u_int64_t));
        }
        char* p = this->pool[IndexType::SRCPORT]->allocate(sizeof(u_int16_t)+sizeof(u_int64_t),disk_block_id);
        memcpy(p,(char*)&(meta.sourcePort),sizeof(u_int16_t));
        memcpy(p+sizeof(u_int16_t),(char*)&position,sizeof(u_int64_t));
        p = this->pool[IndexType::DSTPORT]->allocate(sizeof(u_int16_t)+sizeof(u_int64_t),disk_block_id);
        memcpy(p,(char*)&(meta.destinationPort),sizeof(u_int16_t));
        memcpy(p+sizeof(u_int16_t),(char*)&position,sizeof(u_int64_t));
        return true;
    }

    u_int64_t getIndexLen(u_int32_t type, u_int64_t disk_block_id){
        return this->pool[type]->getLenOfDiskID(disk_block_id);
    }
    
    void writeToBuffer(u_int32_t type, char* buffer, u_int64_t disk_block_id, u_int64_t len, u_int64_t thread_id, u_int64_t disk_pos){
        
    }
};

#endif