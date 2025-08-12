#ifndef DISK_BUFFER_HPP_
#define DISK_BUFFER_HPP_

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "util.hpp"
#include "prefixBloomFilter.hpp"

struct DiskMeta{
    PrefixBloomFilter bloomFilterMeta;
    u_int64_t start_time;
    u_int64_t end_time;
    u_int64_t disk_index_meta[IndexType::TOTAL * 2]; // each type has a start and end index id
    uint64_t packet_count;
    void init(BitMap* bitmap, size_t k){
        this->bloomFilterMeta.init(bitmap, k);
        this->start_time = 0;
        this->end_time = 0;
        this->packet_count = 0;
    }
};

class DiskBuffer {
private:
    const std::string disk_name;
    const u_int64_t block_num;
    DiskMeta* disk_metas;
public:
    DiskBuffer(const std::string& disk_name, u_int64_t block_num, BitMap* bitmap, size_t k)
        : disk_name(disk_name), block_num(block_num) {
        this->disk_metas = (DiskMeta*)mmap(nullptr, block_num * sizeof(DiskMeta), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (this->disk_metas == MAP_FAILED){
            printf("Disk buffer error: mmap failed for disk metas!\n");
            throw std::runtime_error("Disk buffer mmap failed");
        }
        for(u_int64_t i = 0; i < block_num; ++i){
            this->disk_metas[i].init(bitmap, k);
        }
    }
    ~DiskBuffer() {
        munmap((void*)disk_metas, block_num*sizeof(DiskMeta));
    }

    const DiskMeta* getDiskMeta(u_int64_t index)const {
        if (index < block_num) {
            return &disk_metas[index];
        }
        return nullptr;
    }
    void setIndexID(u_int64_t index, IndexType index_type, u_int64_t index_start, u_int64_t index_end){
        if (index < block_num) {
            disk_metas[index].disk_index_meta[index_type * 2] = index_start;
            disk_metas[index].disk_index_meta[index_type * 2 + 1] = index_end;
        } else {
            printf("Disk buffer error: index %lu out of bounds!\n", index);
        }
    }
    void setBloomFilterCol(u_int64_t disk_block_id, u_int64_t col){
        if (disk_block_id >= block_num){
            printf("Disk buffer error: disk_block_id %u out of bounds!\n", disk_block_id);
            return;
        }
        this->disk_metas[disk_block_id].bloomFilterMeta.setWritingCol(col);
    }
    void clearPacketCount(u_int64_t disk_block_id){
        if (disk_block_id < block_num) {
            this->disk_metas[disk_block_id].packet_count = 0;
        } else {
            printf("Disk buffer error: disk_block_id %lu out of bounds!\n", disk_block_id);
        }
    }
    // disk manager thread
    void setTime(u_int64_t disk_block_id, u_int64_t start_time, u_int64_t end_time){
        if (disk_block_id < block_num) {
            this->disk_metas[disk_block_id].start_time = start_time;
            this->disk_metas[disk_block_id].end_time = end_time;
        } else {
            printf("Disk buffer error: disk_block_id %lu out of bounds!\n", index);
        }
    }
    void setPacketCount(u_int64_t disk_block_id, u_int64_t packet_count){
        if (disk_block_id < block_num) {
            this->disk_metas[disk_block_id].packet_count = packet_count;
        } else {
            printf("Disk buffer error: disk_block_id %lu out of bounds!\n", disk_block_id);
        }
    }
};

#endif