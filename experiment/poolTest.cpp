#include "../dpdk_lib/memoryPool.hpp"


#define UNIT_LEN 10lu
#define BUFFER_SIZE 1024lu*1024lu*10lu*UNIT_LEN

char* pool_buffer;
MemoryPool* pool;
char** buffer;
u_int64_t buffer_size;
u_int64_t* buffer_id;

void testPoolAllocate(u_int64_t len_unit, u_int64_t count, u_int64_t block_id_max){
    u_int64_t block_size = count/block_id_max;
    for(u_int64_t i=0; i<count; ++i){
        pool->allocate(len_unit,i/block_size);
    }
}

void testBufferAllocate(u_int64_t len_unit, u_int64_t count, u_int64_t block_id_max){
    u_int64_t block_size = count/block_id_max;
    for(u_int64_t i=0; i<count; ++i){
        buffer[i] = new char[len_unit];
        buffer_id[i] = i/block_size;
    }
}

void testPoolDeallocate(u_int64_t block_id_max){
    for(u_int64_t i=0; i<block_id_max;++i){
        pool->recycle(i);
    }
}

void testBufferDeallocate(u_int64_t block_id_max){
    u_int64_t j = 0;
    for(u_int64_t i=0; i<block_id_max; ++i){
        while(true){
            if(j >= buffer_size || buffer_id[j]!=i){
                break;
            }
            delete buffer[j];
            buffer[j] = nullptr;
            j++;
        }
    }
}

void init(){
    pool_buffer = (char*)mmap(nullptr, BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
    if(pool_buffer == MAP_FAILED){
        printf("Index buffer error: mmap failed for buffer init!\n");
        return;
    }
    pool = new MemoryPool(pool_buffer, BUFFER_SIZE, 1024, UNIT_LEN);
    buffer_size = BUFFER_SIZE/UNIT_LEN;
    buffer = new char*[buffer_size];
    buffer_id = new u_int64_t[buffer_size];
    for(u_int64_t i = 0; i<buffer_size;++i){
        buffer_id[i] = std::numeric_limits<uint64_t>::max();
    }
}

int main(){
    init();
    auto t1 = std::chrono::high_resolution_clock::now();
    auto t2 = std::chrono::high_resolution_clock::now();
    
    t1 = std::chrono::high_resolution_clock::now();
    testPoolAllocate(UNIT_LEN, 1024*1024, 64);
    t2 = std::chrono::high_resolution_clock::now();
    printf("%lu\n",std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());


    t1 = std::chrono::high_resolution_clock::now();
    testBufferAllocate(UNIT_LEN, 1024*1024, 64);
    t2 = std::chrono::high_resolution_clock::now();
    printf("%lu\n",std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());

    t1 = std::chrono::high_resolution_clock::now();
    testPoolDeallocate(64);
    t2 = std::chrono::high_resolution_clock::now();
    printf("%lu\n",std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());

    t1 = std::chrono::high_resolution_clock::now();
    testBufferDeallocate(64);
    t2 = std::chrono::high_resolution_clock::now();
    printf("%lu\n",std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count());
}