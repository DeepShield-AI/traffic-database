#ifndef DISK_AGENT_HPP_
#define DISK_AGENT_HPP_
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <atomic>
#include <libaio.h>
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
    int disk_fd;

    bool* states;
    
};

#endif