#ifndef DUMPER_HPP_
#define DUMPER_HPP_

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>

#include "../lib/wbuffer.hpp"

class Dumper{
private:
    const u_int64_t offset;
    const u_int64_t capacity;

    WBuffer* wbuffer;
    int disk_fd;

    u_int64_t write_place;

    std::atomic_bool stop;

    bool bind_core;
    u_int32_t core_id;

    void bindCore(u_int32_t cpu);
    void dump(char* buffer, u_int64_t size);
public:
    Dumper(WBuffer* wbuffer, int disk_fd, u_int64_t offset, u_int64_t capacity, bool bind_core=false, u_int32_t core_id=0):
        wbuffer(wbuffer), disk_fd(disk_fd), offset(offset), capacity(capacity),bind_core(bind_core),core_id(core_id){
        this->write_place = 0;
        this->stop = true;
    }
    ~Dumper()=default;
    int run();
    void asynchronousStop();
};

#endif