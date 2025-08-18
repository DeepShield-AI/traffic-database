#ifndef PREFIX_BLOOM_FILTER_HPP_
#define PREFIX_BLOOM_FILTER_HPP_
#include <iostream>
#include <vector>
#include <functional>
#include <bitset>
#include "bitMap.hpp"
#include "util.hpp"
#define HASH_SEED 0x5bd1e995

#define IPV4_BLOOM_LEN 1024
#define IPV6_BLOOM_LEN 512

#define SLICE_LEN 8
#define SLICE_VALUE_COUNT (1 << SLICE_LEN)

#define PORT_BIT_LEN (1 << 16)
#define IPV4_BIT_LEN (SLICE_VALUE_COUNT * SLICE_VALUE_COUNT * 2 + SLICE_VALUE_COUNT * IPV4_BLOOM_LEN * 2)
#define IPV6_BIT_LEN (SLICE_VALUE_COUNT * SLICE_VALUE_COUNT + SLICE_VALUE_COUNT * IPV6_BLOOM_LEN * 14)



class PrefixBloomFilter {
private:
    BitMap* bitmap;
    size_t k; // 哈希函数的数量
    u_int64_t writing_col;
    u_int64_t reading_col;

    size_t hashFunction(const std::string& element, size_t i) const {
        std::hash<std::string> hasher;
        auto h = hasher(element) + HASH_SEED * i;
        std::hash<u_int64_t> hasher2;
        return hasher2(h); // 使用不同的种子
    }

    bool setBloom(const std::string& prefix, u_int64_t offset, u_int8_t byte, u_int64_t bloom_len){
        for (size_t i = 0; i< this->k; ++i){
            size_t hash_value = hashFunction(prefix, i) % bloom_len;
            u_int64_t bit_pos = byte * SLICE_VALUE_COUNT + hash_value;
            // printf("%lu of %lu set bit pos: %lu, offset: %lu\n",i,this->k,bit_pos, offset);
            if (!this->bitmap->set(bit_pos + offset, this->writing_col)){
                return false;
            }
        }
        return true;
    }
    bool getBloom(const std::string& prefix, u_int64_t offset, u_int8_t byte, u_int64_t bloom_len) const {
        for (size_t i = 0; i < this->k; ++i){
            size_t hash_value = hashFunction(prefix, i) % bloom_len;
            u_int64_t bit_pos = byte * SLICE_VALUE_COUNT + hash_value;

            if (!this->bitmap->get(bit_pos + offset, this->reading_col)){
                return false;
            }
        }
        return true;
    }

    bool setIPv4(u_int32_t ip, u_int64_t offset){
        u_int64_t bit_pos = (((ip >> (8 * (sizeof(ip) - 1))) & 0xFF) * SLICE_VALUE_COUNT) + ((ip >> (8 * (sizeof(ip) - 2))) & 0xFF);
        // printf("bit_pos: %lu\n",bit_pos);
        if (!this->bitmap->set(bit_pos + offset, this->writing_col)){
            return false;
        }
        bit_pos = (((ip >> (8 * (sizeof(ip) - 3))) & 0xFF) * SLICE_VALUE_COUNT) + ((ip >> (8 * (sizeof(ip) - 4))) & 0xFF);
        // printf("bit_pos: %lu\n",bit_pos);
        if (!this->bitmap->set(bit_pos + offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT, this->writing_col)){
            return false;
        }
        for (int i = sizeof(ip) - 3; i >= 0; --i) {
            uint8_t byte = (ip >> (8 * i)) & 0xFF;
            // printf("byte: %u\n",byte);
            u_int32_t prefix = ip >> (8 * (i + 1));
            std::string prefix_str = std::string((char*)&prefix, sizeof(ip));
            if (!this->setBloom(prefix_str, offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT * 2 + SLICE_VALUE_COUNT * IPV4_BLOOM_LEN * (sizeof(ip) - i - 3), byte, IPV4_BLOOM_LEN)){
                return false;
            }
        }
        return true;
    }
    bool setIPv6(IPv6Address ip, u_int64_t offset){
        // printf("insert ipv6.\n");
        u_int64_t bit_pos = ((((ip >> (8 * (sizeof(ip) - 1)))).low & 0xFF) * SLICE_VALUE_COUNT) + (((ip >> (8 * (sizeof(ip) - 2)))).low & 0xFF);
        if (!this->bitmap->set(bit_pos + offset, this->writing_col)){
            return false;
        }
        // printf("bit_pos: %lu\n",bit_pos);
        for (int i = sizeof(ip) - 3; i >= 0; --i) {
            uint8_t byte = ((ip >> (8 * i))).low & 0xFF;
            // printf("byte: %u\n",byte);
            IPv6Address prefix = ip >> (8 * (i + 1));
            std::string prefix_str = std::string((char*)&prefix, sizeof(ip));
            // printf("begin set byte: %u\n",byte);
            if(!this->setBloom(prefix_str, offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT + SLICE_VALUE_COUNT * IPV6_BLOOM_LEN * (sizeof(ip) - i - 3), byte, IPV6_BLOOM_LEN)){
                return false;
            }
            // printf("set byte: %u\n",byte);
        }
        return true;
    }

    bool getIPv4(u_int32_t ip, u_int64_t offset) const {
        u_int64_t bit_pos = (((ip >> (8 * (sizeof(ip) - 1))) & 0xFF) * SLICE_VALUE_COUNT) + ((ip >> (8 * (sizeof(ip) - 2))) & 0xFF);
        if (!this->bitmap->get(bit_pos + offset, this->reading_col)){
            return false;
        }
        // printf("check a\n");
        bit_pos = (((ip >> (8 * (sizeof(ip) - 3))) & 0xFF) * SLICE_VALUE_COUNT) + ((ip >> (8 * (sizeof(ip) - 4))) & 0xFF);
        if (!this->bitmap->get(bit_pos + offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT, this->reading_col)){
            return false;
        }
        // printf("check b\n");
        for (int i = sizeof(ip) - 3; i >= 0; --i) {
            uint8_t byte = (ip >> (8 * i)) & 0xFF;
            u_int32_t prefix = ip >> (8 * (i + 1));
            std::string prefix_str = std::string((char*)&prefix, sizeof(ip));
            if (!this->getBloom(prefix_str, offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT * 2 + SLICE_VALUE_COUNT * IPV4_BLOOM_LEN * (sizeof(ip) - i - 3), byte, IPV4_BLOOM_LEN)){
                return false;
            }
            // printf("check %u\n",i);
        }
        return true;
    }
    bool getIPv6(IPv6Address ip, u_int64_t offset) const {
        u_int64_t bit_pos = ((((ip >> (8 * (sizeof(ip) - 1)))).low & 0xFF) * SLICE_VALUE_COUNT) + (((ip >> (8 * (sizeof(ip) - 2)))).low & 0xFF);
        if (!this->bitmap->get(bit_pos + offset, this->reading_col)){
            return false;
        }
        for (int i = sizeof(ip) - 3; i >= 0; --i) {
            uint8_t byte = ((ip >> (8 * i))).low & 0xFF;
            IPv6Address prefix = ip >> (8 * (i + 1));
            std::string prefix_str = std::string((char*)&prefix, sizeof(ip));
            if (!this->getBloom(prefix_str, offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT + SLICE_VALUE_COUNT * IPV4_BLOOM_LEN * (sizeof(ip) - i - 3), byte, IPV6_BLOOM_LEN)){
                return false;
            }
        }
        return true;
    }

public:
    PrefixBloomFilter(){
        this->bitmap = nullptr;
        this->k = 0;
        this->writing_col = std::numeric_limits<uint32_t>::max();
        this->reading_col = std::numeric_limits<uint32_t>::max();
    }
    PrefixBloomFilter(BitMap* bitmap, size_t numHashFunctions):
        bitmap(bitmap), k(numHashFunctions), writing_col(0) {
        if(this->bitmap->getRowCount() < (PORT_BIT_LEN + IPV4_BIT_LEN + IPV6_BIT_LEN) * 2){
            printf("PrefixBloomFilter error: bitmap row count %lu is too small!\n", this->bitmap->getRowCount());
        }
        this->writing_col = std::numeric_limits<uint32_t>::max();
        this->reading_col = std::numeric_limits<uint32_t>::max();
    }
    ~PrefixBloomFilter() = default;
    void init(BitMap* bitmap, size_t numHashFunctions){
        this->bitmap = bitmap;
        this->k = numHashFunctions;
        if(this->bitmap->getRowCount() < (PORT_BIT_LEN + IPV4_BIT_LEN + IPV6_BIT_LEN) * 2){
            printf("PrefixBloomFilter error: bitmap row count %lu is too small!\n", this->bitmap->getRowCount());
        }
        this->writing_col = std::numeric_limits<uint32_t>::max();
        this->reading_col = std::numeric_limits<uint32_t>::max();
    }
    BitMap* getBitmap() const {
        return this->bitmap;
    }
    u_int64_t getWritingCol() const {
        if (this->bitmap == nullptr){
            printf("PrefixBloomFilter error: bitmap is not initialized while getWritingCol!\n");
            return std::numeric_limits<u_int64_t>::max();
        }
        return this->writing_col;
    }
    void setWritingCol(u_int64_t col){
        if (this->bitmap == nullptr){
            printf("PrefixBloomFilter error: bitmap is not initialized while setWritingCol!\n");
            return;
        }
        if (col >= this->bitmap->getColCount()){
            printf("PrefixBloomFilter  error: col %lu out of range!\n", col);
            return;
        }
        this->writing_col = col;
    }
    void setReadingCol(u_int64_t col){
        if (this->bitmap == nullptr){
            printf("PrefixBloomFilter error: bitmap is not initialized while setReadingCol!\n");
            return;
        }
        if (col >= this->bitmap->getColCount()){
            printf("PrefixBloomFilter error: col %lu out of range!\n", col);
            return;
        }
        this->reading_col = col;
    }
    bool insertPort(u_int16_t port, IndexType type){
        if (this->bitmap == nullptr){
            printf("PrefixBloomFilter error: bitmap is not initialized while insertPort!\n");
            return false;
        }
        if (type == IndexType::SRCPORT){
            return this->bitmap->set((u_int64_t)port, this->writing_col);
        }
        if (type == IndexType::DSTPORT){
            return this->bitmap->set((u_int64_t)PORT_BIT_LEN + (u_int64_t)port, this->writing_col);
        }
        printf("PrefixBloomFilter error: set invalid port type %d!\n", type);
        return false;
    }
    bool insertIPv4(u_int32_t ip, IndexType type){
        if (this->bitmap == nullptr){
            printf("PrefixBloomFilter error: bitmap is not initialized while insertIPv4!\n");
            return false;
        }
        if (type == IndexType::SRCIP){
            return this->setIPv4(ip, (u_int64_t)PORT_BIT_LEN * 2);
        }
        if (type == IndexType::DSTIP){
            return this->setIPv4(ip, (u_int64_t)PORT_BIT_LEN * 2 + (u_int64_t)IPV4_BIT_LEN);
        }
        printf("PrefixBloomFilter error: set invalid IPv4 type %d!\n", type);
        return false;
    }
    bool insertIPv6(IPv6Address ip, IndexType type){
        if (this->bitmap == nullptr){
            printf("PrefixBloomFilter error: bitmap is not initialized while insertIPv6!\n");
            return false;
        }
        if (type == IndexType::SRCIPv6){
            return this->setIPv6(ip, (u_int64_t)PORT_BIT_LEN * 2 + (u_int64_t)IPV4_BIT_LEN * 2);
        }
        if (type == IndexType::DSTIPv6){
            return this->setIPv6(ip, (u_int64_t)PORT_BIT_LEN * 2 + (u_int64_t)IPV4_BIT_LEN * 2 + (u_int64_t)IPV6_BIT_LEN);
        }
        printf("PrefixBloomFilter error: set invalid IPv6 type %d!\n", type);
        return false;
    }

    bool getPort(u_int16_t port, IndexType type) const {
        if (this->bitmap == nullptr){
            printf("PrefixBloomFilter error: bitmap is not initialized while getPort!\n");
            return false;
        }
        // printf("check port %u\n",port);
        if (type == IndexType::SRCPORT){
            return this->bitmap->get((u_int64_t)port, this->reading_col);
        }
        if (type == IndexType::DSTPORT){
            return this->bitmap->get((u_int64_t)PORT_BIT_LEN + (u_int64_t)port, this->reading_col);
        }
        printf("PrefixBloomFilter error: get invalid port type %d!\n", type);
        return false;
    }
    bool getIPv4(u_int32_t ip, IndexType type) const {
        if (this->bitmap == nullptr){
            printf("PrefixBloomFilter error: bitmap is not initialized while getIPv4!\n");
            return false;
        }
        // printf("check ip %u.%u.%u.%u\n",(ip >> 24),(ip>>16)&0xff,(ip>>8)&0xff,ip&0xff);
        if (type == IndexType::SRCIP){
            return this->getIPv4(ip, (u_int64_t)PORT_BIT_LEN * 2);
        }
        if (type == IndexType::DSTIP){
            return this->getIPv4(ip, (u_int64_t)PORT_BIT_LEN * 2 + (u_int64_t)IPV4_BIT_LEN);
        }
        printf("PrefixBloomFilter error: get invalid IPv4 type %d!\n", type);
        return false;
    }
    bool getIPv6(IPv6Address ip, IndexType type) const {
        if (this->bitmap == nullptr){
            printf("PrefixBloomFilter error: bitmap is not initialized while getIPv6!\n");
            return false;
        }
        if (type == IndexType::SRCIPv6){
            return this->getIPv6(ip, (u_int64_t)PORT_BIT_LEN * 2 + (u_int64_t)IPV4_BIT_LEN * 2);
        }
        if (type == IndexType::DSTIP){
            return this->getIPv6(ip, (u_int64_t)PORT_BIT_LEN * 2 + (u_int64_t)IPV4_BIT_LEN * 2 + (u_int64_t)IPV6_BIT_LEN);
        }
        printf("PrefixBloomFilter error: get invalid IPv4 type %d!\n", type);
        return false;
    }
};

#endif