#ifndef PACKETAGGREGATOR_HPP_
#define PACKETAGGREGATOR_HPP_
#include <unordered_map>
#include <string>
#include <chrono>
// #include "util.hpp"

struct FlowAttr{
    u_int64_t lastOffset;
    u_int64_t ts;
};

struct FlowMetadata{
    std::string sourceAddress;
    std::string destinationAddress;
    u_int16_t sourcePort;
    u_int16_t destinationPort;
    struct hash {
        size_t operator()(const FlowMetadata& f) const {
            return std::hash<u_int16_t>()(f.sourcePort) ^ std::hash<u_int16_t>()(f.destinationPort) ^
                    std::hash<std::string>()(f.sourceAddress) ^ std::hash<std::string>()(f.destinationAddress);
        }
    };
    bool operator==(const FlowMetadata& f) const {
        return sourceAddress == f.sourceAddress && destinationAddress == f.destinationAddress && 
                sourcePort == f.sourcePort && destinationPort == f.destinationPort;
    }
};

class PacketAggregator{
    const u_int64_t offsetThreshold;
    const u_int64_t tsThreshold;

    //hash table
    std::unordered_map<FlowMetadata, FlowAttr, FlowMetadata::hash> aggMap;
public:
    PacketAggregator(u_int64_t _offsetThreshold, u_int64_t _tsThreshold):offsetThreshold(_offsetThreshold),tsThreshold(_tsThreshold){
        this->aggMap = std::unordered_map<FlowMetadata, FlowAttr, FlowMetadata::hash>();
    }
    ~PacketAggregator()=default;
    u_int64_t addPacket(FlowMetadata meta, u_int64_t offset, u_int64_t ts, u_int64_t disk_size){
        u_int64_t last = std::numeric_limits<uint64_t>::max();
        auto it = this->aggMap.find(meta);
        if(it == this->aggMap.end()){
            FlowAttr flow = {
                .lastOffset = offset,
                .ts = ts,
            };
            this->aggMap.insert(std::make_pair(meta,flow));
            return last;
        }
        u_int64_t ordered_offset = offset <= it->second.lastOffset ? offset + disk_size: offset;
        // u_int64_t last_queue_id = it->second.lastOffset >> 32;
        if(ordered_offset - it->second.lastOffset > this->offsetThreshold || ts - it->second.ts > this->tsThreshold){
            it->second.lastOffset = offset;
            it->second.ts = ts;
            return last;
        }
        // u_int64_t last_start_time = block_queue[last_queue_id]->start_time;

        // The data block is replaced or the interval time exceeds the threshold.
        // The starting time of the new data block must be higher than any time of the old data block.
        // if(it->second.ts < last_start_time || ts - it->second.ts > this->tsThreshold){
        //     it->second.lastOffset = offset;
        //     it->second.ts = ts;
        //     return last;
        // }
        last = it->second.lastOffset;
        it->second.lastOffset = offset;
        it->second.ts = ts;
        return last;
    }
};

#endif