#ifndef CHECKER_HPP_
#define CHECKER_HPP_
#include <unordered_map>
#include <vector>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include "../lib/flowbuffer.hpp"
#include "../../../dpdk_lib/pointerRingBuffer.hpp"

class FlowChecker{
private:
    const u_int64_t flow_buffer_time_threshold;
    std::vector<std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash>*> flow_maps;
    std::vector<PointerRingBuffer*> flowToWbufferRings;

    std::atomic_bool stop;

    bool bind_core;
    u_int32_t core_id;

    void bindCore(u_int32_t cpu);
    void check(u_int64_t i);
public:
    FlowChecker(u_int64_t flow_buffer_time_threshold, bool bind_core = false, u_int32_t core_id = 0):
        flow_buffer_time_threshold(flow_buffer_time_threshold),bind_core(bind_core),core_id(core_id){
        this->flow_maps = std::vector<std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash>*>();
        this->flowToWbufferRings = std::vector<PointerRingBuffer*>();
        this->stop = true;
    }
    ~FlowChecker() = default;
    void addCheckMap(std::unordered_map<FlowMetadata, FlowBuffer*, FlowMetadata::hash>* map, PointerRingBuffer* ring);
    int run();
    void asynchronousStop();
};

#endif