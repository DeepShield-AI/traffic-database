#ifndef INDEX_BLOCK_BUFFER_HPP_
#define INDEX_BLOCK_BUFFER_HPP_
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <atomic>
#include <algorithm>

class IndexBlockBuffer{
private:
    const u_int64_t total_block_num;
    const u_int64_t block_size;
    const u_int64_t buffer_size;
    const u_int64_t disk_block_num;
public:
};

#endif