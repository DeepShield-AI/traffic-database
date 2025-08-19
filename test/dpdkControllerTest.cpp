#include "../dpdk_component/controller.hpp"
#include <iostream>
#include <string>
#include <fstream>

#define TIMES 2

// const u_int8_t pcap_head[] = {0xd4,0xc3,0xb2,0xa1,0x02,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
//                             0x00,0x00,0x04,0x00,0x65,0x00,0x00,0x00};
// const u_int8_t pcap_head[] = {0xd4,0xc3,0xb2,0xa1,0x02,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
//                             0xff,0xff,0x00,0x00,0x01,0x00,0x00,0x00};
const u_int32_t index_ring_capacity = 1024*1024*32;
// const u_int32_t storage_ring_capacity = 1024;
// const u_int64_t truncate_interval = ((u_int64_t)1)<<32;
// const u_int32_t pcap_header_len = 24;
const u_int32_t eth_header_len = 14;
const size_t hash_num = 4;
// const u_int64_t file_capacity = 1024*1024*1024;
u_int16_t nb_rx = 4;
u_int64_t data_disk_size = 1024lu*1024lu*1024lu*1024lu;
u_int64_t data_block_size = 1024lu*1024lu*1024lu;
u_int64_t index_disk_size = 256lu*1024lu*1024lu*1024lu;
u_int64_t index_block_size = 1024lu*1024lu*1024lu;
u_int64_t memory_pool_capacity = 1024lu*1024lu*1024lu;
u_int64_t memory_pool_list_len_each = 1024;

u_int64_t data_block_cache_num = 16;
u_int64_t index_buffer_cache_num = 64;
u_int64_t index_block_cache_num = 4;
u_int64_t delay_threshold = 1;
u_int64_t bitmap_backup_col_num = 8;

u_int32_t index_construct_thread_num = 4;
u_int32_t index_persist_thread_num = 2;
const u_int32_t data_disk_manager_thread_num = 1;
const u_int32_t index_disk_manager_thread_num = 1;
const u_int32_t data_memory_manager_thread_num = 1;
const u_int32_t index_memory_manager_thread_num = 1;
const u_int32_t data_agent_num_each = 1;
const u_int32_t index_agent_num_each = 1;
const u_int32_t agent_ring_depth = 1024;
const u_int32_t agent_ring_idle_time = 1024;

std::string data_disk_name = "/dev/sdb";
u_int64_t data_disk_offset = 0;
std::string index_disk_name = "/dev/sdb";
u_int64_t index_disk_offset = data_disk_offset + data_disk_size;

bool bind_core = true;
// u_int32_t controller_core_id = 0;

// const u_int32_t direct_storage_thread_num = 2;
// const u_int32_t index_storage_thread_num = 1;
// const u_int32_t max_node = 1024*1024*4;
// const std::string bpf_prog_name = "./bpf/tag.o";

int main(){
   Controller* controller = new Controller();

   InitData init_data;

   // std::cout << "Do you want to bind to cores? (y/n)" << std::endl;
   // char bind;
   // std::cin >> bind;
   // if(bind == 'y'){
   //    init_data.bind_core = true;
   //    std::cout << "Enter the controller core number (0 is remained)" << std::endl;
   //    std::cin >> init_data.controller_core_id;
   // }else{
   //    init_data.bind_core = false;
   //    init_data.controller_core_id = 0;
   // }

   // std::cout << "Enter number of DPDK packet capture threads" << std::endl;
   // std::cin >> nb_rx;
   // init_data.nb_rx = nb_rx;

   // if(init_data.bind_core){
   //    // std::cout << "Enter the core number for each DPDK packet capture threads (0 is remained)" << std::endl;
   //    // for(int i=0;i<nb_rx;++i){
   //    //    u_int32_t core_id;
   //    //    std::cin >> core_id;
   //    //    init_data.dpdk_core_id_list.push_back(core_id);
   //    // }
   //    std::cout << "Enter the core number for each packet processing threads (0 is remained)" << std::endl;
   //    for(int i=0;i<nb_rx;++i){
   //       u_int32_t core_id;
   //       std::cin >> core_id;
   //       init_data.dpdk_core_id_list.push_back(core_id);
   //       init_data.packet_core_id_list.push_back(core_id);
   //    }
   // }

   // std::cout << "Enter number of indexing thread" << std::endl;
   // std::cin >> index_thread_num;

   // init_data.index_thread_num = index_thread_num;

   // if(init_data.bind_core){
   //    std::cout << "Enter the core number for each indexing threads (0 is remained)" << std::endl;
   //    for(int i=0;i<index_thread_num;++i){
   //       u_int32_t core_id;
   //       std::cin >> core_id;
   //       init_data.indexing_core_id_list.push_back(core_id);
   //    }
   // }

   init_data.index_ring_capacity = index_ring_capacity;
   // init_data.pcap_header_len = pcap_header_len;
   init_data.eth_header_len = eth_header_len;
   // init_data.file_capacity = file_capacity;
   init_data.hash_num = hash_num;
   init_data.data_disk_size = data_disk_size;
   init_data.data_block_size = data_block_size;
   init_data.index_disk_size = index_disk_size;
   init_data.index_block_size = index_block_size;

   init_data.memory_pool_capacity_each = memory_pool_capacity/nb_rx;
   init_data.memory_pool_list_len_each = memory_pool_list_len_each;

   init_data.data_block_cache_num = data_block_cache_num;
   init_data.index_buffer_cache_num = index_buffer_cache_num;
   init_data.index_block_cache_num = index_block_cache_num;
   init_data.delay_threshold = delay_threshold;

   init_data.bitmap_backup_col_num = bitmap_backup_col_num;

   init_data.agent_ring_depth = agent_ring_depth;
   init_data.agent_ring_idle_time = agent_ring_idle_time;

   init_data.data_disk_name = data_disk_name;
   init_data.data_disk_offset = data_disk_offset;
   init_data.index_disk_name = index_disk_name;
   init_data.index_disk_offset = index_disk_offset;

   init_data.bind_core = bind_core;
   init_data.nb_rx = nb_rx;
   init_data.index_construct_thread_num = index_construct_thread_num;
   init_data.index_persist_thread_num = index_persist_thread_num;
   init_data.data_disk_manager_thread_num = data_disk_manager_thread_num;
   init_data.index_disk_manager_thread_num = index_disk_manager_thread_num;
   init_data.data_memory_manager_thread_num = data_memory_manager_thread_num;
   init_data.index_memory_manager_thread_num = index_memory_manager_thread_num;
   init_data.data_agent_num_each = data_agent_num_each;
   init_data.index_agent_num_each = index_agent_num_each;

   init_data.controller_core_id = 2;
   init_data.dpdk_core_id_list = std::vector<u_int32_t>({4,6,8,10});
   init_data.packet_core_id_list = std::vector<u_int32_t>({4,6,8,10});
   init_data.indexing_core_id_list = std::vector<u_int32_t>({20,22,24,26});
   init_data.persisting_core_id_list = std::vector<u_int32_t>({28,30});
   init_data.data_dumping_core_id_list = std::vector<u_int32_t>({32});
   init_data.index_dumping_core_id_list = std::vector<u_int32_t>({34});
   init_data.data_kernel_core_id_list = std::vector<u_int32_t>({36});
   init_data.index_kernel_core_id_list = std::vector<u_int32_t>({38});
   init_data.data_recycling_core_id_list =std::vector<u_int32_t>({40});
   init_data.index_recycling_core_id_list = std::vector<u_int32_t>({42});

   for (u_int32_t i = 0; i<init_data.data_agent_num_each * init_data.index_disk_manager_thread_num; ++i){
      init_data.data_kernel_core_id_list.push_back(i*2 +48);
   }
   for (u_int32_t i = 0; i<init_data.data_agent_num_each * init_data.index_disk_manager_thread_num; ++i){
      init_data.index_kernel_core_id_list.push_back(i*2 + 48 + init_data.data_agent_num_each * init_data.index_disk_manager_thread_num * 2);
   }

   controller->init(init_data);
   controller->run();
}