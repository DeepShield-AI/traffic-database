#ifndef DPDK_HPP_
#define DPDK_HPP_

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_bpf_ethdev.h>
#include <rte_mbuf.h>
#include <rte_flow.h>
#include <vector>
#include <iostream>

#define RX_RING_SIZE 512
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 512
#define BURST_SIZE 32

static const struct rte_bpf_xsym bpf_xsym[] = {
	{
		.name = RTE_STR(stdout),
		.type = RTE_BPF_XTYPE_VAR,
		.var = {
			.val = (void *)(uintptr_t)&stdout,
			.desc = {
				.type = RTE_BPF_ARG_PTR,
				.size = sizeof(stdout),
			},
		},
	},
	{
		.name = RTE_STR(rte_pktmbuf_dump),
		.type = RTE_BPF_XTYPE_FUNC,
		.func = {
			.val = (uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))rte_pktmbuf_dump,
			.nb_args = 3,
			.args = {
				[0] = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uintptr_t),
				},
				[1] = {
					.type = RTE_BPF_ARG_PTR_MBUF,
					.size = sizeof(struct rte_mbuf),
				},
				[2] = {
					.type = RTE_BPF_ARG_RAW,
					.size = sizeof(uint32_t),
				},
			},
		},
	},
};

static struct rte_eth_conf port_conf = {
	.rxmode = {
		.split_hdr_size = 0,
        .mq_mode = RTE_ETH_MQ_RX_RSS,
	},
	.txmode = {
		.mq_mode = RTE_ETH_MQ_TX_NONE,
	},
    .rx_adv_conf = {
        .rss_conf = {
            .rss_key = NULL,  // Use default RSS key
            .rss_hf = ETH_RSS_IP | ETH_RSS_TCP | ETH_RSS_UDP,  // Hash on IP, TCP, and UDP
        },
    },
};
// static struct rte_eth_conf port_conf = {
// 	.rxmode = {
// 		.split_hdr_size = 0,
// 	},
// 	.txmode = {
// 		.mq_mode = RTE_ETH_MQ_TX_NONE,
// 	},
// };

class DPDK{
    u_int16_t nb_ports;
    u_int16_t nb_rx_queue;
    u_int16_t nb_tx_queue;
    std::vector<rte_ether_addr> mac_vector;
    struct rte_mempool *mbuf_pool;

    std::string cores;

    bool dpdkInitPort(u_int16_t port_id){
        if(port_id >= this->nb_ports){
            printf("DPDK error: dpdkInitPort with wrong port id.\n");
            return false;
        }
        int ret;
        struct rte_eth_dev_info dev_info;
        struct rte_eth_rxconf rx_conf = {
            .rx_thresh = { .pthresh = 8, .hthresh = 8, .wthresh = 4 },
	        .rx_free_thresh = 32,
	        .rx_drop_en = 0,
	        .rx_deferred_start = 0,
	        .rx_nseg = 0,
        };
        struct rte_eth_conf local_port_conf = port_conf;

        ret = rte_eth_dev_info_get(port_id, &dev_info);
        if (ret != 0) {
            printf("DPDK error: rte_eth_dev_info_get fail!\n");
            return false;
        }
        // printf("Supported RSS hash functions: 0x%" PRIx64 "\n", dev_info.flow_type_rss_offloads);
        uint64_t DESIRED_RSS_HF = RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP;
        uint64_t valid_rss_hf = dev_info.flow_type_rss_offloads & DESIRED_RSS_HF;
        local_port_conf.rx_adv_conf.rss_conf.rss_hf = valid_rss_hf;
        
        ret = rte_eth_dev_configure(port_id, this->nb_rx_queue, this->nb_tx_queue, &local_port_conf);
        if (ret < 0) {
            printf("DPDK error: rte_eth_dev_configure fail!\n");
            return false;
        }
        // printf("rte_eth_dev_configure done with port_id %u, nb_rx %u, nb_tx %u.\n",port_id, this->nb_rx_queue,this->nb_tx_queue);
        for(u_int16_t i = 0; i<this->nb_rx_queue; ++i){
            ret = rte_eth_rx_queue_setup(port_id, i, RX_RING_SIZE, rte_eth_dev_socket_id(port_id), &rx_conf, this->mbuf_pool);
            // printf("rte_eth_rx_queue_setup with ret %d.\n",ret);
            if (ret < 0) {
                printf("DPDK error: rte_eth_rx_queue_setup while set up %u.\n",i);
                return false;
            }
            
        }
        for(u_int16_t i = 0; i<this->nb_tx_queue; ++i){
            ret = rte_eth_tx_queue_setup(port_id, i, TX_RING_SIZE, rte_eth_dev_socket_id(port_id), NULL);
            if (ret < 0) {
                printf("DPDK error: rte_eth_tx_queue_setup while set up %u.\n",i);
                return false;
            }
        }

        ret = rte_eth_dev_start(port_id);

        if (ret < 0) {
            printf("DPDK error: rte_eth_dev_start fail!\n");
            return false;
        }

        ret = rte_eth_promiscuous_enable(port_id);

        if (ret < 0) {
            printf("DPDK error: rte_eth_promiscuous_enable fail!\n");
            return false;
        }

        ret = rte_eth_macaddr_get(port_id,&(this->mac_vector[port_id]));
        if(ret < 0){
            printf("DPDK error: rte_eth_macaddr_get fail!\n");
            return false;
        }
        return true;
    }

    bool dpdkInit(){
        int ret;
        char* pro_name = "main";
        char* para_name = "-l";
        char* a[3];
        a[0] = pro_name;
        u_int32_t cores_size = this->cores.size() + 1;  // include '\0'
        a[1] = para_name;
        a[2] = new char[cores_size];              
        memcpy(a[2],this->cores.c_str(),cores_size);
        ret = rte_eal_init(3,a);
        // delete a[2];
	    if (ret < 0){
		    rte_panic("Cannot init EAL\n");
        }
        this->mbuf_pool = rte_pktmbuf_pool_create("mbuf_pool", NUM_MBUFS,
		    MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE,
		    rte_socket_id());
        if (this->mbuf_pool == NULL) {
            return false;
        }

        this->nb_ports = rte_eth_dev_count_avail();
        if (this->nb_ports == 0) {
            return false;
        }
        printf("DPDK log: There're %u ports.\n",this->nb_ports);
        return true;
    }
public:
    DPDK(u_int16_t nb_rx_queue, u_int16_t nb_tx_queue, bool bind_core, std::vector<u_int32_t> cores){
        this->nb_rx_queue = nb_rx_queue;
        this->nb_tx_queue = nb_tx_queue;
        this->mbuf_pool = NULL;
        this->cores = "";
        if(bind_core){
            this->cores += std::to_string(0);
            for(auto core:cores){
                this->cores += ",";
                this->cores += std::to_string(core);
            }
        }
        else{
            for(int i=0;i<=this->nb_rx_queue;++i){
                this->cores += std::to_string(i);
                if(i!=this->nb_rx_queue){
                    this->cores += ",";
                }
            }
        }
        printf("DPDK log: work cores is %s.\n",this->cores.c_str());
        if(!this->dpdkInit()){
            printf("DPDK error: cannot init dpdk!\n");
            this->nb_ports = 0;
            return;
        }
        this->mac_vector = std::vector<rte_ether_addr>(this->nb_ports);
        for(u_int16_t i= 0;i < this->nb_ports;++i){
            if(!this->dpdkInitPort(i)){
                printf("DPDK error: cannot init dpdk port %u!\n",i);
                this->nb_ports = 0;
                return;
            }
        }
        for(u_int16_t i= 0;i < this->nb_ports;++i){
            printf("DPDK log: port %u mac is %02X:%02X:%02X:%02X:%02X:%02X\n",
                i,
                this->mac_vector[i].addr_bytes[0],
                this->mac_vector[i].addr_bytes[1],
                this->mac_vector[i].addr_bytes[2],
                this->mac_vector[i].addr_bytes[3],
                this->mac_vector[i].addr_bytes[4],
                this->mac_vector[i].addr_bytes[5]);
        }
    }
    ~DPDK(){
        rte_eal_cleanup();
    }
    u_int16_t getNbPorts()const{
        return this->nb_ports;
    }
    int loadBPF(u_int16_t port_id, u_int16_t rx_id, std::string fname){
        int32_t rc;
	    uint32_t flags;
	    struct rte_bpf_prm prm;
    	const char *sname;

    	memset(&prm, 0, sizeof(prm));
    	prm.xsym = bpf_xsym;
    	prm.nb_xsym = RTE_DIM(bpf_xsym);

        prm.prog_arg.type = RTE_BPF_ARG_PTR_MBUF;
        prm.prog_arg.size = sizeof(struct rte_mbuf);
        prm.prog_arg.buf_size = RTE_MBUF_DEFAULT_BUF_SIZE;

        // prm.prog_arg.type = RTE_BPF_ARG_PTR;
	    // prm.prog_arg.size = RTE_MBUF_DEFAULT_BUF_SIZE;
        // flags |= RTE_BPF_ETH_F_JIT;
    
        

    	sname = ".text";

    	rc = rte_bpf_eth_rx_elf_load(port_id, rx_id, &prm, fname.c_str(), sname, flags);
        if(rc!=0){
    		printf("%d:%s\n", rc, strerror(-rc));
        }
    	
        return rc;
    }
    void unloadBPF(u_int16_t port_id, u_int16_t rx_id){
        rte_bpf_eth_rx_unload(port_id, rx_id);
    }
    int getRXBurst(struct rte_mbuf **bufs, u_int16_t port_id, u_int16_t rx_id){
        if(port_id >= this->nb_ports || rx_id >= this->nb_rx_queue){
            printf("DPDK error: getRXBurst with wrong port id or rx id: %u-%u, %u-%u.\n",port_id,this->nb_ports,rx_id,this->nb_rx_queue);
            return -1;
        }
        return rte_eth_rx_burst(port_id, rx_id, bufs, BURST_SIZE);
    }
};

#endif