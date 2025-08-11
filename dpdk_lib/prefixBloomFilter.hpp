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
        return hasher(element) + i * HASH_SEED; // 使用不同的种子
    }

    void setBloom(const std::string& prefix, u_int64_t offset, u_int8_t byte){
        for (size_t i = 0; i< this->k; ++i){
            size_t hash_value = hashFunction(prefix, i);
            u_int64_t bit_pos = byte * SLICE_VALUE_COUNT + hash_value;

            this->bitmap->set(bit_pos + offset, this->writing_col);
        }
    }
    bool getBloom(const std::string& prefix, u_int64_t offset, u_int8_t byte) const {
        for (size_t i = 0; i < this->k; ++i){
            size_t hash_value = hashFunction(prefix, i);
            u_int64_t bit_pos = byte * SLICE_VALUE_COUNT + hash_value;

            if (!this->bitmap->get(bit_pos + offset, this->reading_col)){
                return false;
            }
        }
        return true;
    }

    void setIPv4(u_int32_t ip, u_int64_t offset){
        u_int64_t bit_pos = ((ip >> (8 * (sizeof(ip) - 1))) & 0xFF * SLICE_VALUE_COUNT) + ((ip >> (8 * (sizeof(ip) - 2))) & 0xFF);
        this->bitmap->set(bit_pos + offset, this->writing_col);
        bit_pos = ((ip >> (8 * (sizeof(ip) - 3))) & 0xFF * SLICE_VALUE_COUNT) + ((ip >> (8 * (sizeof(ip) - 4))) & 0xFF);
        this->bitmap->set(bit_pos + offset, this->writing_col);
        for (int i = sizeof(ip) - 3; i >= 0; --i) {
            uint8_t byte = (ip >> (8 * i)) & 0xFF;
            u_int32_t prefix = ip >> (8 * (i + 1));
            std::string prefix_str = std::string((char*)&prefix, sizeof(ip));
            this->setBloom(prefix_str, offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT * 2 + SLICE_VALUE_COUNT * IPV4_BLOOM_LEN * (sizeof(ip) - i - 3), byte);
        }
    }
    void setIPv6(IPv6Address ip, u_int64_t offset){
        u_int64_t bit_pos = (((ip >> (8 * (sizeof(ip) - 1)))).low & 0xFF * SLICE_VALUE_COUNT) + (((ip >> (8 * (sizeof(ip) - 2)))).low & 0xFF);
        this->bitmap->set(bit_pos + offset, this->writing_col);
        for (int i = sizeof(ip) - 3; i >= 0; --i) {
            uint8_t byte = ((ip >> (8 * i))).low & 0xFF;
            IPv6Address prefix = ip >> (8 * (i + 1));
            std::string prefix_str = std::string((char*)&prefix, sizeof(ip));
            this->setBloom(prefix_str, offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT + SLICE_VALUE_COUNT * IPV4_BLOOM_LEN * (sizeof(ip) - i - 3), byte);
        }
    }

    bool getIPv4(u_int32_t ip, u_int64_t offset) const {
        u_int64_t bit_pos = ((ip >> (8 * (sizeof(ip) - 1))) & 0xFF * SLICE_VALUE_COUNT) + ((ip >> (8 * (sizeof(ip) - 2))) & 0xFF);
        if (!this->bitmap->get(bit_pos + offset, this->reading_col)){
            return false;
        }
        bit_pos = ((ip >> (8 * (sizeof(ip) - 3))) & 0xFF * SLICE_VALUE_COUNT) + ((ip >> (8 * (sizeof(ip) - 4))) & 0xFF);
        if (!this->bitmap->get(bit_pos + offset, this->reading_col)){
            return false;
        }
        for (int i = sizeof(ip) - 3; i >= 0; --i) {
            uint8_t byte = (ip >> (8 * i)) & 0xFF;
            u_int32_t prefix = ip >> (8 * (i + 1));
            std::string prefix_str = std::string((char*)&prefix, sizeof(ip));
            if (!this->getBloom(prefix_str, offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT * 2 + SLICE_VALUE_COUNT * IPV4_BLOOM_LEN * (sizeof(ip) - i - 3), byte)){
                return false;
            }
        }
        return true;
    }
    bool getIPv6(IPv6Address ip, u_int64_t offset) const {
        u_int64_t bit_pos = (((ip >> (8 * (sizeof(ip) - 1)))).low & 0xFF * SLICE_VALUE_COUNT) + (((ip >> (8 * (sizeof(ip) - 2)))).low & 0xFF);
        if (!this->bitmap->get(bit_pos + offset, this->reading_col)){
            return false;
        }
        for (int i = sizeof(ip) - 3; i >= 0; --i) {
            uint8_t byte = ((ip >> (8 * i))).low & 0xFF;
            IPv6Address prefix = ip >> (8 * (i + 1));
            std::string prefix_str = std::string((char*)&prefix, sizeof(ip));
            if (!this->getBloom(prefix_str, offset + SLICE_VALUE_COUNT * SLICE_VALUE_COUNT + SLICE_VALUE_COUNT * IPV4_BLOOM_LEN * (sizeof(ip) - i - 3), byte)){
                return false;
            }
        }
        return true;
    }

public:
    PrefixBloomFilter(BitMap* bitmap, size_t numHashFunctions):
        bitmap(bitmap), k(numHashFunctions), writing_col(0) {
        if(this->bitmap->getRowCount() < (PORT_BIT_LEN + IPV4_BIT_LEN + IPV6_BIT_LEN) * 2){
            printf("PrefixBloomFilter error: bitmap row count %llu is too small!\n", this->bitmap->getRowCount());
        }
        this->writing_col = std::numeric_limits<uint32_t>::max();
        this->reading_col = std::numeric_limits<uint32_t>::max();
    }
    ~PrefixBloomFilter() = default;
    void setWritingCol(u_int64_t col){
        if (col >= this->bitmap->getColCount()){
            printf("PrefixBloomFilter error: col %llu out of range!\n", col);
            return;
        }
        this->writing_col = col;
    }
    void setReadingCol(u_int64_t col){
        if (col >= this->bitmap->getColCount()){
            printf("PrefixBloomFilter error: col %llu out of range!\n", col);
            return;
        }
        this->reading_col = col;
    }
    void insertPort(u_int16_t port, IndexType type){
        if (type == IndexType::SRCPORT){
            this->bitmap->set((u_int64_t)port, this->writing_col);
            return;
        }
        if (type == IndexType::DSTPORT){
            this->bitmap->set((u_int64_t)PORT_BIT_LEN + (u_int64_t)port, this->writing_col);
            return;
        }
        printf("PrefixBloomFilter error: set invalid port type %d!\n", type);
    }
    void insertIPv4(u_int32_t ip, IndexType type){
        if (type == IndexType::SRCIP){
            this->setIPv4(ip, (u_int64_t)PORT_BIT_LEN * 2);
            return;
        }
        if (type == IndexType::DSTIP){
            this->setIPv4(ip, (u_int64_t)PORT_BIT_LEN * 2 + (u_int64_t)IPV4_BIT_LEN);
            return;
        }
        printf("PrefixBloomFilter error: set invalid IPv4 type %d!\n", type);
    }
    void insertIPv6(IPv6Address ip, IndexType type){
        if (type == IndexType::SRCIPv6){
            this->setIPv6(ip, (u_int64_t)PORT_BIT_LEN * 2 + (u_int64_t)IPV4_BIT_LEN * 2);
            return;
        }
        if (type == IndexType::DSTIPv6){
            this->setIPv6(ip, (u_int64_t)PORT_BIT_LEN * 2 + (u_int64_t)IPV4_BIT_LEN * 2 + (u_int64_t)IPV6_BIT_LEN);
            return;
        }
        printf("PrefixBloomFilter error: set invalid IPv6 type %d!\n", type);
    }

    bool getPort(u_int16_t port, IndexType type) const {
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