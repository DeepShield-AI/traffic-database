#include "../component/controller.hpp"
#include <iostream>
#include <string>
#include <fstream>

u_int32_t eth_header_len = 14;
u_int32_t flow_ring_capacity = 1024*1024*32;

u_int64_t data_disk_size = 1024lu*1024lu*1024lu*1024lu;
u_int64_t data_block_size = 1024lu*1024lu*1024lu;

u_int64_t memory_pool_capacity = 1024lu*1024lu*1024lu*16lu;

u_int16_t nb_rx = 4;

u_int64_t wbuffer_size_each = 1024lu*1024lu*1024lu*4lu;

u_int64_t flow_buffer_len_threshold = 1024lu*1024lu;
u_int64_t flow_buffer_time_threshold = rte_get_tsc_hz()/100;

u_int64_t checker_thread_num = 1;

std::string data_disk_name = "/dev/sdb";
u_int64_t data_disk_offset = 0;

bool bind_core = true;
u_int32_t controller_core_id = 2;
std::vector<u_int32_t> dpdk_core_id_list = std::vector<u_int32_t>({4,6,8,10});
std::vector<u_int32_t> packet_core_id_list = std::vector<u_int32_t>({4,6,8,10});
std::vector<u_int32_t> data_dumping_core_id_list = std::vector<u_int32_t>({20,22,24,26});
std::vector<u_int32_t> checking_core_id_list = std::vector<u_int32_t>({36});

int main(){
    Controller* controller = new Controller();

    InitData init_data;

    init_data.flow_ring_capacity = flow_ring_capacity;
    init_data.nb_rx = nb_rx;
    init_data.eth_header_len = eth_header_len;
    
    init_data.data_disk_size = data_disk_size;
    init_data.data_block_size = data_block_size;

    init_data.memory_pool_capacity = memory_pool_capacity;
    
    init_data.wbuffer_size_each = wbuffer_size_each;

    init_data.flow_buffer_len_threshold = flow_buffer_len_threshold;
    init_data.flow_buffer_time_threshold = flow_buffer_time_threshold;

    init_data.checker_thread_num = checker_thread_num;

    init_data.data_disk_name = data_disk_name;
    init_data.data_disk_offset = data_disk_offset;

    init_data.bind_core = bind_core;
    init_data.controller_core_id = controller_core_id;
    init_data.dpdk_core_id_list = dpdk_core_id_list;
    init_data.packet_core_id_list = packet_core_id_list;

    init_data.data_dumping_core_id_list = data_dumping_core_id_list;
    init_data.checking_core_id_list = checking_core_id_list;

    controller->init(init_data);
    controller->run();
}
