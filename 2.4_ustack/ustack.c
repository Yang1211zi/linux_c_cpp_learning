#include<stdio.h>
#include<rte_eal.h>
#include<rte_ethdev.h>

#define NUM_MBUFS 4096
#define BURST_SIZE 129
#define ENABLE_SEND 1

#if ENABLE_SEND
uint8_t smac[RTE_ETHER_ADDR_LEN];
uint8_t dmac[RTE_ETHER_ADDR_LEN];

uint32_t sip;
uint32_t dip;

uint16_t sport;
uint16_t dport;
#endif

int global_port_id=0;
static struct rte_eth_conf port_c={//port_c配置参数对象，专门告诉 DPDK“这张网卡我要怎么配置”，网卡配置单
    .rxmode={
        .max_rx_pkt_len=RTE_ETHER_MAX_LEN,
    },
};

/*
nb_sys_ports为dpdk绑定的网卡的数量，后续的数据就从这个网卡接收
*/
static int ustack_init_port(struct rte_mempool *mbuf_pool){
    uint16_t nb_sys_ports = rte_eth_dev_count_avail();//返回当前 DPDK 可以使用的 Ethernet 设备数量
    if(nb_sys_ports==0){
        rte_exit(EXIT_FAILURE,"No supported eth found\n");
        printf("nb_sys_ports:%d\n",nb_sys_ports);
    }
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(global_port_id,&dev_info);//获取网卡信息
    const int num_rx_queues=1;
    #if ENABLE_SEND
    const int num_tx_queues=1;
    #else
    const int num_tx_queues=0;
    #endif
    rte_eth_dev_configure(global_port_id,num_rx_queues,num_tx_queues,&port_c);//对网卡进行总体配置
    /*
    rte_eth_dev_configure(
    port_id,       // 哪张网卡
    nb_rx_queue,   // RX队列数量
    nb_tx_queue,   // TX队列数量
    eth_conf       // 网卡配置参数
);
    */
    if(rte_eth_rx_queue_setup(global_port_id,0,128,rte_eth_dev_socket_id(global_port_id),NULL,mbuf_pool)<0){//真正创建和配置一个 RX Queue
        rte_exit(EXIT_FAILURE,"Cannot setup rx queue\n");
    }
    #if ENABLE_SEND
    struct rte_eth_txconf txconf = dev_info.default_txconf;
    txconf.offloads = port_c.txmode.offloads;
    if(rte_eth_tx_queue_setup(global_port_id,0,512,rte_eth_dev_socket_id(global_port_id),&txconf)<0){
        rte_exit(EXIT_FAILURE,"Cannot setup tx queue\n");
    }
    #endif

    if(rte_eth_dev_start(global_port_id)<0){//启动网卡
        rte_exit(EXIT_FAILURE,"Cannot start port\n");
    }
    return 0;
}

static int ustack_encode_udp_pkt(uint8_t*msg,uint8_t*data,uint16_t tol_len){
    struct rte_ether_hdr *ethhdr = (struct rte_ether_hdr *)msg;
    rte_memcpy(ethhdr->s_addr.addr_bytes, smac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(ethhdr->d_addr.addr_bytes, dmac, RTE_ETHER_ADDR_LEN);
    ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
}

int main(int argc,char *argv[]){
    if(rte_eal_init(argc,argv)<0){//eal:环境抽象层，初始化DPDK环境，配置巨页，UIO/VFIO等参数
        rte_exit(EXIT_FAILURE,"Error with EAL initialization\n");
    }
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS, 0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if(mbuf_pool==NULL){
        rte_exit(EXIT_FAILURE,"Cannot create mbuf pool\n");
    }
    ustack_init_port(mbuf_pool);

    while(1){
        struct rte_mbuf *bufs[BURST_SIZE];
        const uint16_t nb_rx = rte_eth_rx_burst(global_port_id, 0, bufs, BURST_SIZE);
        if(nb_rx>BURST_SIZE){
            rte_exit(EXIT_FAILURE,"error\n");
        }
        if(nb_rx>0){
            printf("Received %u packets\n",nb_rx);
            for(int i=0;i<nb_rx;i++){
                struct rte_ether_hdr *ethhdr = rte_pktmbuf_mtod(bufs[i], struct rte_ether_hdr *);
			if (ethhdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
				continue;
			}

			struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(bufs[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
			if (iphdr->next_proto_id == IPPROTO_UDP) {

				struct rte_udp_hdr *udphdr = (struct rte_udp_hdr *)(iphdr + 1);

            #if ENABLE_SEND
            rte_memcpy(smac, ethhdr->s_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
            rte_memcpy(dmac, ethhdr->d_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
            rte_memcpy(&sip, &iphdr->dst_addr, sizeof(uint32_t));
            rte_memcpy(&dip, &iphdr->src_addr, sizeof(uint32_t));
            rte_memcpy(&sport, &udphdr->dst_port, sizeof(uint16_t));
            rte_memcpy(&dport, &udphdr->src_port, sizeof(uint16_t));
            rte_eth_tx_burst(global_port_id, 0, &bufs[i], 1);

            struct in_addr addr;
				addr.s_addr = iphdr->src_addr;
				printf("sip %s:%d --> ", inet_ntoa(addr), ntohs(udphdr->src_port));
                //inet_ntoa() 把 IPv4 的二进制地址转换成 "192.168.1.10" 这样的字符串

				addr.s_addr = iphdr->dst_addr;
				printf("dip %s:%d --> ", inet_ntoa(addr), ntohs(udphdr->dst_port));
                //ntohs() 把网络字节序的端口号转换成主机字节序的端口号

            #endif
            }
        }
    }

    printf("hello dpdk\n");
    return 0;
}
}