#ifndef DISK_AGENT_HPP_
#define DISK_AGENT_HPP_
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <atomic>
#include <liburing.h>
#include "util.hpp"

// class StateRing{
// private:
//     const u_int32_t ring_size;
//     std::atomic_bool* states;
// public:
//     StateRing(u_int32_t size):ring_size(size){
//         this->states = new std::atomic_bool(this->ring_size);
//     }
//     ~StateRing(){
//         delete[] this->states;
//     }
//     bool getState(u_int32_t id) const {
//         id = id % this->ring_size;
//         return this->states[id].load();
//     }
// };

class DiskAgent{
private:
    // const std::string disk_name;
    const u_int64_t disk_size;
    // const u_int64_t start_offset;
    const u_int64_t block_size;
    const u_int64_t block_num;
    const u_int32_t ring_depth;
    const u_int64_t basic_offset;

    int disk_fd;
    struct io_uring_params params;
    
    struct io_uring* ring;
public:
    DiskAgent(u_int64_t disk_size, u_int64_t block_size, u_int64_t basic_offset, int disk_fd, u_int32_t ring_depth, u_int32_t idle_time)
        : disk_size(disk_size), block_size(block_size), block_num(disk_size / block_size), ring_depth(ring_depth), basic_offset(basic_offset), disk_fd(disk_fd){
        if (this->disk_size % this->block_size != 0){
            printf("Disk agent error: disk size %lu is not a multiple of block size %lu!\n", this->disk_size, this->block_size);
            throw std::runtime_error("Disk size wrong");
        }
        // this->disk_fd = open(this->disk_name.c_str(), O_DIRECT | O_RDWR);
        // if (this->disk_fd < 0) {
        //     printf("Disk agent error: failed to open disk %s!\n", this->disk_name.c_str());
        //     throw std::runtime_error("Disk open failed");
        // }

        this->params = {
            .flags = IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF,
            .sq_thread_idle = idle_time,
        };
        this->ring = nullptr;
        // if (io_uring_queue_init(this->ring_depth, &this->ring, 0) < 0) {
        //     printf("Disk agent error: failed to init io_uring!\n");
        //     throw std::runtime_error("io_uring init failed");
        // }
    }
    ~DiskAgent() {
        if (this->ring != nullptr){
            // while (io_uring_sq_ready(this->ring)>0);
            io_uring_queue_exit(this->ring);
            delete this->ring;
            this->ring = nullptr;
        }
        // close(this->disk_fd);
    }

    int getDiskFd() const { return this->disk_fd; }
    // u_int64_t getStartOffset() const { return this->start_offset; }
    u_int32_t getBlockSize() const { return this->block_size; }
    u_int64_t getBlockNum() const { return this->block_num; }
    u_int64_t getDiskSize() const { return this->disk_size; }
    u_int64_t getBasicOffset() const { return this->basic_offset; }
    
    bool kernel_run(u_int32_t cpu_core) {
        this->params.sq_thread_cpu = cpu_core;
        this->ring = new io_uring;
        if (io_uring_queue_init_params(this->ring_depth, this->ring, &params) < 0) {
            printf("Disk agent error: failed to init io_uring with SQPOLL!\n");
            throw std::runtime_error("io_uring init failed");
            return false;
        }
        printf("Disk Agent log: run on kernel %u.\n",cpu_core);
        return true;
    }

    // bool write(char* buffer, u_int64_t write_pos){
    //     if (write_pos > this->block_num){
    //         printf("Disk agent error: write position %llu exceeds block number %llu!\n", write_pos, this->block_num);
    //         return false;
    //     }
    //     struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
    //     if (!sqe) {
    //         printf("Disk agent error: Failed to get SQE!\n");
    //         return false;
    //     }
    //     u_int64_t offset = this->start_offset + write_pos * this->block_size;
    //     io_uring_prep_write(sqe, this->disk_fd, buffer, this->block_size, offset);
    //     sqe->user_data = write_pos;
    //     int ret = io_uring_submit(ring);
    //     if (ret < 0) {
    //         printf("Disk agent error: io_uring_submit failed!\n");
    //         return false;
    //     }

    //     struct io_uring_cqe* cqe;
    //     ret = io_uring_wait_cqe(ring, &cqe);
    //     if (ret < 0 || cqe->res < 0) {
    //         printf("Disk agent error: io_uring write failed!\n");
    //         return false;
    //     }

    //     io_uring_cqe_seen(ring, cqe);
    //     return true;
    // }
    bool asyncWrite(char* buffer, u_int64_t block_id, u_int64_t write_pos) {
        if (write_pos > this->block_num){
            printf("Disk agent error: asyncWrite position %lu exceeds block number %lu!\n", write_pos, this->block_num);
            return false;
        }
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        if (!sqe) {
            printf("Disk agent error: Failed to get SQE!\n");
            return false;
        }
        u_int64_t offset = write_pos * this->block_size;
        io_uring_prep_write(sqe, this->disk_fd, buffer, this->block_size, offset + this->basic_offset);
        sqe->user_data = block_id;

        int ret = io_uring_submit(ring);
        if (ret < 0) {
            printf("Disk agent error: io_uring_submit failed!\n");
            return false;
        }

        printf("Disk agent log: writing block %lu to position %lu with basic offset %lu.\n",block_id,write_pos,this->basic_offset);

        return true;
    }
    u_int64_t processCompletions() {
        struct io_uring_cqe* cqe = nullptr;
        if(io_uring_peek_cqe(ring, &cqe) == 0 && cqe != nullptr) {
            if (cqe->res < 0) {
                printf("Disk agent error: IO operation failed: %s\n", strerror(-cqe->res));
                return std::numeric_limits<u_int64_t>::max();
            }
            io_uring_cqe_seen(ring, cqe);
            return cqe->user_data;
        }
        return std::numeric_limits<u_int64_t>::max();
    }
    bool read(char* buffer, u_int64_t id){
        if (id >= this->block_num) {
            printf("Disk agent error: read id %lu exceeds block number %lu!\n", id, this->block_num);
            return false;
        }
        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        if (!sqe) {
            printf("Disk agent error: Failed to get SQE for read!\n");
            return false;
        }

        u_int64_t offset = id * this->block_size;

        printf("read offset %lu\n",offset + this->basic_offset);

        // printf("sqe: %lu, disk_fd: %d, buffer: %lu, blocksize: %lu.\n",(u_int64_t)sqe,this->disk_fd,(u_int64_t)buffer,this->block_size);

        io_uring_prep_read(sqe, this->disk_fd, buffer, this->block_size, offset + this->basic_offset);
        sqe->user_data = id;

        int ret = io_uring_submit(ring);
        if (ret < 0) {
            printf("Disk agent error: io_uring_submit (read) failed!\n");
            return false;
        }

        struct io_uring_cqe* cqe = nullptr;
        ret = io_uring_wait_cqe(ring, &cqe);
        if (ret < 0 || cqe->res < 0) {
            printf("Disk agent error: io_uring read failed:ret=%d cqe->res=%d (%s)!\n",ret, cqe->res, strerror(-cqe->res));
            return false;
        }

        io_uring_cqe_seen(ring, cqe);
        // printf("read offset %lu done\n",offset + this->basic_offset);
        return true;
    }
    bool read(char* buffer, u_int64_t begin_id, u_int64_t end_id){
        if (begin_id >= this->block_num || end_id > this->block_num || begin_id > end_id) {
            printf("Disk agent error: read range %lu - %lu exceeds block number %lu or is invalid!\n", begin_id, end_id, this->block_num);
            return false;
        }

        struct io_uring_sqe* sqe = io_uring_get_sqe(ring);
        if (!sqe) {
            printf("Disk agent error: Failed to get SQE for read!\n");
            return false;
        }

        u_int64_t offset = begin_id * this->block_size;

        io_uring_prep_read(sqe, this->disk_fd, buffer, (end_id - begin_id + 1)*this->block_size, offset + this->basic_offset);
        sqe->user_data = begin_id;

        int ret = io_uring_submit(ring);
        if (ret < 0) {
            printf("Disk agent error: io_uring_submit (read) failed!\n");
            return false;
        }

        struct io_uring_cqe* cqe = nullptr;
        ret = io_uring_wait_cqe(ring, &cqe);
        if (ret < 0 || cqe->res < 0) {
            printf("Disk agent error: io_uring read failed:ret=%d cqe->res=%d (%s)!\n",ret, cqe->res, strerror(-cqe->res));
            return false;
        }

        io_uring_cqe_seen(ring, cqe);
        return true;
        
    }
    bool jobFinished(){
        return io_uring_sq_ready(this->ring) == 0 && io_uring_cq_ready(this->ring) == 0;
    }
};

#endif