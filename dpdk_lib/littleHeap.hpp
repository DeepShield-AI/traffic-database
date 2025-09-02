#ifndef LITTLE_HEAP_HPP_
#define LITTLE_HEAP_HPP_
#include <iostream>
#include "util.hpp"

template <class KeyType>
struct LittleHeapNode{
    KeyType key;
    u_int64_t value;
    u_int64_t leftChild;
    u_int64_t rightChild;
    LittleHeapNode(){
        this->leftChild = std::numeric_limits<u_int64_t>::max();
        this->rightChild = std::numeric_limits<u_int64_t>::max();
    }
};

class LittleHeap{
private:
    char* buffer;
    const u_int64_t buffer_size;
    u_int64_t heads[IndexType::TOTAL_INDEX];
public:
};

#endif