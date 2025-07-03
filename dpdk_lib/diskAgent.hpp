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
    const std::string disk_name;
    const u_int64_t disk_size;
    const u_int64_t start_offset;
    const u_int32_t block_size;
    const u_int64_t block_num;
    const u_int32_t ring_depth;

    int disk_fd;
    u_int32_t write_pos;
    struct io_uring ring;
public:
    DiskAgent(const std::string& disk_name, u_int64_t disk_size, u_int64_t start_offset, u_int32_t block_size, u_int32_t ring_depth)
        : disk_name(disk_name), disk_size(disk_size), start_offset(start_offset), block_size(block_size), block_num(disk_size / block_size), ring_depth(ring_depth), write_pos(0) {
        if (this->disk_size % this->block_size != 0){
            printf("Disk agent error: disk size %llu is not a multiple of block size %u!\n", this->disk_size, this->block_size);
            throw std::runtime_error("Disk size wrong");
        }
        this->disk_fd = open(this->disk_name.c_str(), O_DIRECT | O_RDWR);
        if (this->disk_fd < 0) {
            printf("Disk agent error: failed to open disk %s!\n", this->disk_name.c_str());
            throw std::runtime_error("Disk open failed");
        }
        if (io_uring_queue_init(this->ring_depth, &this->ring, 0) < 0) {
            printf("Disk agent error: failed to init io_uring!\n");
            throw std::runtime_error("io_uring init failed");
        }
    }
    ~DiskAgent() {
        io_uring_queue_exit(&this->ring);
        close(this->disk_fd);
    }

    int getDiskFd() const { return this->disk_fd; }
    u_int64_t getStartOffset() const { return this->start_offset; }
    u_int32_t getBlockSize() const { return this->block_size; }
    u_int64_t getBlockNum() const { return this->block_num; }
    u_int64_t getDiskSize() const { return this->disk_size; }
    
    u_int32_t write(char* buffer){
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            printf("Disk agent error: Failed to get SQE!\n");
            return std::numeric_limits<u_int32_t>::max();
        }
        u_int64_t offset = this->start_offset + this->write_pos * this->block_size;
        io_uring_prep_write(sqe, this->disk_fd, buffer, this->block_size, offset);
        sqe->user_data = this->write_pos;
        int ret = io_uring_submit(&ring);
        if (ret < 0) {
            printf("Disk agent error: io_uring_submit failed!\n");
            return std::numeric_limits<u_int32_t>::max();
        }

        struct io_uring_cqe* cqe;
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0 || cqe->res < 0) {
            printf("Disk agent error: io_uring write failed!\n");
            return std::numeric_limits<u_int32_t>::max();
        }

        io_uring_cqe_seen(&ring, cqe);

        u_int32_t current_pos = this->write_pos;
        this->write_pos = (this->write_pos + 1) % this->block_num;
        return current_pos;
    }
    u_int32_t asyncWrite(char* buffer, u_int32_t block_id) {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            printf("Disk agent error: Failed to get SQE!\n");
            return std::numeric_limits<u_int32_t>::max();
        }
        u_int64_t offset = this->start_offset + this->write_pos * this->block_size;
        io_uring_prep_write(sqe, this->disk_fd, buffer, this->block_size, offset);
        sqe->user_data = block_id;

        int ret = io_uring_submit(&ring);
        if (ret < 0) {
            printf("Disk agent error: io_uring_submit failed!\n");
            return std::numeric_limits<u_int32_t>::max();
        }

        u_int32_t current_pos = this->write_pos;
        this->write_pos = (this->write_pos + 1) % this->block_num;
        return current_pos;
    }
    u_int32_t processCompletions() {
        struct io_uring_cqe* cqe = nullptr;
        if(io_uring_peek_cqe(&ring, &cqe) == 0 && cqe != nullptr) {
            if (cqe->res < 0) {
                printf("Disk agent error: IO operation failed: %s\n", strerror(-cqe->res));
                return std::numeric_limits<u_int32_t>::max();
            }
            io_uring_cqe_seen(&ring, cqe);
            return cqe->user_data;
        }
        return std::numeric_limits<u_int32_t>::max();
    }
    bool read(char* buffer, u_int32_t id){
        if (id >= this->block_num) {
            printf("Disk agent error: read id %u exceeds block number %llu!\n", id, this->block_num);
            return false;
        }
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            printf("Disk agent error: Failed to get SQE for read!\n");
            return false;
        }

        u_int64_t offset = this->start_offset + id * this->block_size;
        io_uring_prep_read(sqe, this->disk_fd, buffer, this->block_size, offset);
        sqe->user_data = id;

        int ret = io_uring_submit(&ring);
        if (ret < 0) {
            printf("Disk agent error: io_uring_submit (read) failed:!\n");
            return false;
        }

        struct io_uring_cqe* cqe;
        ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0 || cqe->res < 0) {
            printf("Disk agent error: io_uring read failed!\n");
            return false;
        }

        io_uring_cqe_seen(&ring, cqe);
        return true;
    }
};

#endif