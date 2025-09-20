#include "../dpdk_component/indexGenerator.hpp"

#include <iostream>
#include <thread>
#include <vector>
#include <numa.h>
#include <numaif.h>
#define THREAD_NUM 8
#define WAIT_TIME 2

struct IndexStore{
    u_int64_t ts;
    u_int64_t disk_block_id;
    u_int64_t position;
    u_int64_t rx_id;
    char sourceAddress[16];
    char destinationAddress[16];
    u_int16_t sourcePort;
    u_int16_t destinationPort;
    u_int32_t version;
};

#define BUFFER_SIZE 1024lu*1024lu*1024lu
#define INDEX_ENUM_LEN sizeof(IndexStore)
#define DATA_OFFSET BUFFER_SIZE*64
#define INDEX_OFFSET BUFFER_SIZE*80
#define INDEX_SIZE 2040336

const u_int32_t index_ring_capacity = 1024*1024*32;
u_int64_t data_disk_size = 1024lu*1024lu*1024lu*64lu;
u_int64_t data_block_size = 1024lu*1024lu*1024lu;
u_int64_t index_disk_size = 256lu*1024lu*1024lu*1024lu;
u_int64_t index_block_size = 1024lu*1024lu*1024lu;
u_int64_t bitmap_backup_col_num = 8;

u_int64_t memory_pool_capacity = 1024lu*1024lu*1024lu;
u_int64_t memory_pool_list_len_each = 1024;
u_int64_t index_buffer_cache_num = 64;
u_int64_t index_block_cache_num = 4;
const size_t hash_num = 4;

std::vector<u_int32_t> indexing_core_id_list = std::vector<u_int32_t>({4,6,8,10,12,14,16,18});

std::vector<PointerRingBuffer*>* indexRings;
BitMap* bitmap;
IndexBuffer* indexBuffer;
IndexBlockBuffer* indexBlockBuffer;
std::vector<IndexMemoryPool*>* indexMemoryPools;

std::vector<IndexGenerator*> indexGenerators;
std::vector<std::thread*> indexGeneratorThreads;


void init(){
    indexRings = new std::vector<PointerRingBuffer*>();
    for(u_int64_t i = 0; i<THREAD_NUM; ++i){
        PointerRingBuffer* indexRing = new PointerRingBuffer(index_ring_capacity);
        indexRings->push_back(indexRing);
    }
    bitmap = new BitMap((PORT_BIT_LEN + IPV4_BIT_LEN + IPV6_BIT_LEN) * 2, data_disk_size / data_block_size, bitmap_backup_col_num);

    indexMemoryPools = new std::vector<IndexMemoryPool*>();

    for (u_int32_t i=0; i<THREAD_NUM; ++i){
        IndexMemoryPool* mp = new IndexMemoryPool(memory_pool_capacity/THREAD_NUM, memory_pool_list_len_each);
        indexMemoryPools->push_back(mp);
    }

    indexBuffer = new IndexBuffer(index_buffer_cache_num, data_disk_size / data_block_size, bitmap, hash_num ,indexMemoryPools);
    indexBlockBuffer = new IndexBlockBuffer(index_block_cache_num, index_block_size, index_disk_size / index_block_size, THREAD_NUM);

    indexGenerators = std::vector<IndexGenerator*>();
    indexGeneratorThreads = std::vector<std::thread*>();

    for(u_int64_t i=0; i<THREAD_NUM; ++i){
        IndexGenerator* ig = new IndexGenerator(indexBuffer, i, true, indexing_core_id_list[i]);
        ig->addRingBuffer((*(indexRings))[i]);
        indexGenerators.push_back(ig);
    }
}

void fill(){
    const char* filename = "/dev/sdb";
    int read_fd = open(filename, O_RDONLY);
    if (read_fd < 0) {
        perror("open for read");
        return;
    }

    char* buffer = new char[INDEX_SIZE];

    ssize_t ret = pread(read_fd, buffer, INDEX_SIZE, INDEX_OFFSET);
    if (ret < 0) perror("pread");

    u_int64_t count = 4;
    for(u_int64_t i = 0; i<INDEX_SIZE; i+=INDEX_ENUM_LEN){
        IndexStore* index_store = (IndexStore*)(buffer + i);

        Index* index = new Index();
        index->disk_block_id = index_store->position / data_block_size;
        index->position = index_store->position;
        index->ts = index->ts;
        index->meta.sourceAddress = std::string(index_store->sourceAddress,index_store->version);
        index->meta.destinationAddress = std::string(index_store->destinationAddress,index_store->version);
        index->meta.sourcePort = index_store->sourcePort;
        index->meta.destinationPort = index_store->destinationPort;
        
        // if (count % 10000 == 0){
        //     printf("%lu %lu\n",count,index->position);
        // }
        count ++;
        index->rx_id = count % THREAD_NUM;
        (*(indexRings))[count % THREAD_NUM]->put(index);
    }
}

void run(){
    for(auto ig:indexGenerators){
        std::thread* t = new std::thread(&IndexGenerator::run,ig);
        indexGeneratorThreads.push_back(t);
    }
}

void stop(){
    sleep(WAIT_TIME);
    for(auto ir:*indexRings){
        ir->asynchronousStop();
    }
    for(u_int32_t i=0;i<indexGenerators.size();++i){
        indexGenerators[i]->asynchronousStop();
        indexGeneratorThreads[i]->join();
    }
}

int main(){
    init();
    fill();
    run();
    stop();
    return 0;
}