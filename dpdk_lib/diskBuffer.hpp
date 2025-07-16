#ifndef DISK_BUFFER_HPP_
#define DISK_BUFFER_HPP_

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "util.hpp"

// struct IndexDiskMeta{
//     u_int32_t srcip_start;
//     u_int32_t srcip_end;
//     u_int32_t dstip_start;
//     u_int32_t dstip_end;
//     u_int32_t srcport_start;
//     u_int32_t srcport_end;
//     u_int32_t dstport_start;
//     u_int32_t dstport_end;
//     u_int32_t srcipv6_start;
//     u_int32_t srcipv6_end;
//     u_int32_t dstipv6_start;
//     u_int32_t dstipv6_end;
//     u_int32_t qua_start;
//     u_int32_t qua_end;
//     u_int32_t qua_start;
//     u_int32_t quaivpv_end;
// };

struct DiskMeta{
    // u_int32_t rss_id;
    // u_int32_t next_id; 
    // u_int32_t bitmap_id;
    // u_int32_t padding;
    u_int64_t start_time;
    u_int64_t end_time;
    u_int64_t index_meta[IndexType::TOTAL * 2]; // each type has a start and end index id
};

class DiskBuffer {
private:
    const std::string disk_name;
    const u_int64_t block_num;
    DiskMeta* disk_metas;
public:
    DiskBuffer(const std::string& disk_name, u_int64_t block_num)
        : disk_name(disk_name), block_num(block_num) {
        this->disk_metas = (DiskMeta*)mmap(nullptr, block_num * sizeof(DiskMeta), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        if (this->disk_metas == MAP_FAILED){
            printf("Disk buffer error: mmap failed for disk metas!\n");
            throw std::runtime_error("Disk buffer mmap failed");
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
    // disk manager thread
    // void setRSSID(u_int64_t index, u_int32_t rss_id){
    //     if (index < block_num) {
    //         disk_metas[index].rss_id = rss_id;
    //     } else {
    //         printf("Disk buffer error: index %lu out of bounds!\n", index);
    //     }
    // }
    // disk manager thread
    // void setBitmapID(u_int64_t index, u_int32_t bitmap_id){
    //     if (index < block_num) {
    //         disk_metas[index].bitmap_id = bitmap_id;
    //     } else {
    //         printf("Disk buffer error: index %lu out of bounds!\n", index);
    //     }
    // }
    // disk manager thread
    // void setNextID(u_int64_t index, u_int32_t next_id){
    //     if (index < block_num) {
    //         disk_metas[index].next_id = next_id;
    //     } else {
    //         printf("Disk buffer error: index %lu out of bounds!\n", index);
    //     }
    // }
    // index storage thread
    void setIndexID(u_int64_t index, IndexType index_type, u_int64_t index_start, u_int64_t index_end){
        if (index < block_num) {
            disk_metas[index].index_meta[index_type * 2] = index_start;
            disk_metas[index].index_meta[index_type * 2 + 1] = index_end;
        } else {
            printf("Disk buffer error: index %lu out of bounds!\n", index);
        }
    }
    // disk manager thread
    void setTime(u_int64_t index, u_int64_t start_time, u_int64_t end_time){
        if (index < block_num) {
            disk_metas[index].start_time = start_time;
            disk_metas[index].end_time = end_time;
        } else {
            printf("Disk buffer error: index %lu out of bounds!\n", index);
        }
    }
};

#endif