#ifndef QUERIER_HPP_
#define QUERIER_HPP_
#include <vector>
#include <string>
#include <list>
#include <chrono>
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "../dpdk_lib/header.hpp"
#include "../dpdk_lib/util.hpp"
#include "../dpdk_lib/diskBuffer.hpp"
#include "../dpdk_lib/diskAgent.hpp"
// #include "../lib/shareBuffer.hpp"
// #include "../lib/arrayList.hpp"
// #include "storage.hpp"

struct Answer{
    u_int32_t block_id;
    std::list<u_int64_t> pointers;
};

struct AtomKey{
    u_int32_t cachePos;
    std::string key;
};

struct QueryTreeNode{
    std::string exp;
    std::list<QueryTreeNode*> children;
};

class QueryTree{
    std::string originExpression;
    QueryTreeNode* root;
    std::list<std::string> expList;

    void clearTree(QueryTreeNode* node);
    std::list<std::string> splitExpression();
    bool grammarVerify(std::list<std::string> exp_list);
    bool grammarVerifySimply(std::list<std::string> exp_list);
    // std::list<std::string> ExpressionFormat(std::list<std::string> exp_list);
    // void treeConstruct(std::list<std::string> exp_list, QueryTree* treeRoot);
public:
    QueryTree(){
        this->originExpression = std::string();
        this->root = nullptr;
    }
    ~QueryTree(){
        this->clearTree(this->root);
    }
    bool inputExpression(std::string exp);
    std::list<std::string> getExpList();
    // std::string to_string();
};

class Querier{
    std::string expression;
    std::string outputFilename;
    // std::string pcapHeader;

    u_int64_t startTime;
    u_int64_t endTime;
    u_int64_t packet_count;

    u_int64_t indexBlockSize;
    u_int64_t dataBlockSize;
    u_int64_t readBlockSize;
    u_int64_t cellSize;

    u_int64_t indexBufferSize;
    u_int64_t dataBufferSize;

    // const std::string index_name[FLOW_META_INDEX_NUM] = {
    //     "./data/index/pcap.pcap_srcip_idx",
    //     "./data/index/pcap.pcap_dstip_idx",
    //     "./data/index/pcap.pcap_srcport_idx",
    //     "./data/index/pcap.pcap_dstport_idx",
    // };
    // const std::string pointer_name = "./data/index/pcap.pcappt";
    // const std::string data_name = "./data/index/pcap.pcap";

    // shared memory, read only
    // ShareBuffer* packetBuffer;
    // ArrayList<u_int32_t>* packetPointer;
    // std::vector<SkipList*>* flowMetaIndexCaches;
    // std::vector<StorageMeta>* storageMetas;
    
    QueryTree tree;

    char* indexBuffer;
    char* dataBuffer;

    DiskBuffer* diskBuffer;

    DiskAgent* indexAgent;
    DiskAgent* dataAgent;    

    void intersect(std::list<u_int64_t>& la, std::list<u_int64_t>& lb);
    void join(std::list<u_int64_t>& la, std::list<u_int64_t>& lb);
    void intersect(std::list<Answer>& la, std::list<Answer>& lb);
    void join(std::list<Answer>& la, std::list<Answer>& lb);

    std::list<std::string> decomposeExpression();
    std::vector<u_int64_t> getIndexRange(AtomKey key);
    std::vector<u_int64_t> getOffsetList(std::vector<u_int64_t>& index_list, AtomKey key);
    void readPackets(std::vector<u_int64_t>& offset_list);
    std::list<Answer> getPointerByFlowMetaIndex(AtomKey key);
    std::list<Answer> getPointerByFlowMetaRange(AtomKey startKey,AtomKey endKey);
    std::list<Answer> searchExpression(std::list<std::string> exp_list);
    char* mmapFile(int fileFD, u_int64_t fileSize);
    void closeFile(int fileFD, char* buffer, u_int64_t fileSize);
    u_int64_t getFileSize(int fileFD);
    void outputPacketToFile(std::list<Answer> flowHeadList);
    bool runUnit();
public:
    Querier(DiskBuffer* diskBuffer, DiskAgent* indexAgent, DiskAgent* dataAgent, u_int64_t index_block_size, u_int64_t data_block_size, u_int64_t read_block_size, u_int64_t cell_size):
        diskBuffer(diskBuffer),indexAgent(indexAgent),dataAgent(dataAgent),indexBlockSize(index_block_size),dataBlockSize(data_block_size),readBlockSize(read_block_size),cellSize(cell_size){
        // std::cout << "Querier construct." <<std::endl;
        // this->packetBuffer = packetBuffer;
        // this->packetPointer = packetPointer;
        // this->flowMetaIndexCaches = flowMetaIndexCaches;
        this->expression = std::string();
        this->outputFilename = std::string();
        // this->pcapHeader = pcapHeader;
        // this->storageMetas = storageMetas;
        this->tree = QueryTree();
        // this->indexBuffer = new char[this->indexAgent->getBlockSize()*2];
        // this->dataBuffer = new char[this->dataAgent->getBlockSize()];

        this->indexBufferSize = this->indexBlockSize * 2;
        this->dataBufferSize = this->dataBlockSize * 3;

        if (posix_memalign((void**)&(this->indexBuffer), 4096, this->indexBufferSize)) {
            perror("posix_memalign");
            exit(1);
        }
        memset(this->indexBuffer,0,this->indexBufferSize);
        if (posix_memalign((void**)&(this->dataBuffer), 4096, this->dataBufferSize)) {
            perror("posix_memalign");
            exit(1);
        }
        memset(this->dataBuffer,0,this->dataBufferSize);
        // std::cout << "Querier construct end." <<std::endl;
    }
    ~Querier()=default;
    void input(std::string expression, std::string outputFilename, std::string start_time, std::string end_time);
    void run();
};

#endif