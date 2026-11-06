#include<stdio.h>
#include<rte_eal.h>
#include<rte_ethdev.h>

#define NUM_MBUFS 4096
#define BURST_SIZE 1024
#define ENABLE_SEND 1
#define ENABLE_TCP  1
#define ECHO_SEND 0
#define NO_ECHO 1
#define ENABLE_ARP 1
#define ENABLE_ICMP 1
#define ENABLE_ARP_REPLY 1
#define ENABLE_TIMER 1
#define ENABLE_RINGBUFFER 1
#define ENABLE_MULTHREAD 1
#define ENABLE_UDP_APP 1
#define ENABLE_TCP_APP 1
#define ENABLE_KNI_APP 1
#define TCP_WINDOW_SIZE 14600
#define RING_SIZE 1024
#define UDP_APP_SIZE 1024

#if ENABLE_SEND
uint8_t smac[RTE_ETHER_ADDR_LEN];
uint8_t dmac[RTE_ETHER_ADDR_LEN];

uint32_t sip;
uint32_t dip;

uint16_t sport;
uint16_t dport;
#endif

#if ENABLE_TIMER
static uint8_t gDefaultArpMac[RTE_ETHER_ADDR_LEN] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
//
#endif

#if ENABLE_RINGBUFFER
   struct inout_ring{
    struct rte_ring *in;
    struct rte_ring *out;
   }

   static struct inout_ring*rinst=NULL;
   static struct inout_ring*rinstance(void){
    if(rinst==NULL){
        rinst=rte_malloc("inout_ring",sizeof(struct inout_ring),0);
        memset(rinst,0,sizeof(struct inout_ring));
    }
    return rinst;
   }

#if ENABLE_ARP
   #define ARP_ENTRY_STATUS_DYNAMIC	0
    #define ARP_ENTRY_STATUS_STATIC		1


    #define LL_ADD(item, list) do {		\
        item->prev = NULL;				\
        item->next = list;				\
        if (list != NULL) list->prev = item; \
        list = item;					\
    } while(0)


    #define LL_REMOVE(item, list) do {		\
        if (item->prev != NULL) item->prev->next = item->next;	\
        if (item->next != NULL) item->next->prev = item->prev;	\
        if (list == item) list = item->next;	\
        item->prev = item->next = NULL;			\
    } while(0)

    struct arp_entry{//ARP 表中的一条记录
        uint32_t ip;
        uint8_t mac[RTE_ETHER_ADDR_LEN];
        uint8_t status;
        arp_entry*prev;
        arp_entry*next;
    }

    struct arp_table {

        struct arp_entry *entries;
        int count;

    };

    static arp_table *arpt = NULL;

    static arp_table*arp_table_instance(void){
        if(arpt==NULL){
            arpt=(arp_table*)malloc(sizeof(arp_table));
            arpt->entries=NULL;
            arpt->count=0;
            if(arpt==NULL){
                rte_exit(EXIT_FAILURE,"Cannot create arp_table\n");
            }
        }
        return arpt;
    }

    static uint8_t*arp_table_mac_lookup(uint32_t ip){
        arp_entry*item;
        arp_table*table=arp_table_instance();
        for(item=table->entries;item!=NULL;item=item->next){
            if(item->ip==ip){
                return item->mac;
    } 
    }
    return NULL;
    }

   #endif

#if ENABLE_UDP_APP
//每条链接对应一个localhost节点，localhost节点中包含了该连接的本地ip、本地端口、协议类型、收发缓冲区等信息
    struct localhost{
        int fd;
        uint32_t localip;
        int localport;

        uint8_t localmac[RTE_ETHER_ADDR_LEN];

        int protocol;

        struct rte_ring*rbuf;
        struct rte_ring*sbuf;

        struct localhost*prev;
        struct localhost*next;

        pthread_cond_t cond;
	    pthread_mutex_t mutex;

    }

    struct localhost*lhost=NULL;//loaclhost链表的头指针,每个连接对应一个localhost节点

#endif

#if ENABLE_TCP
uint8_t flags;
uint32_t seqnum;
uint32_t acknum;

typedef enum __USTACK_TCP_STATUS {

	USTACK_TCP_STATUS_CLOSED = 0,
	USTACK_TCP_STATUS_LISTEN,
	USTACK_TCP_STATUS_SYN_RCVD,
	USTACK_TCP_STATUS_SYN_SENT,
	USTACK_TCP_STATUS_ESTABLISHED,
	USTACK_TCP_STATUS_FIN_WAIT_1,
	USTACK_TCP_STATUS_FIN_WAIT_2,
	USTACK_TCP_STATUS_CLOSING,
	USTACK_TCP_STATUS_TIMEWAIT,
	USTACK_TCP_STATUS_CLOSE_WAIT,
	USTACK_TCP_STATUS_LAST_ACK
	
} USTACK_TCP_STATUS;

uint8_t tcp_status = USTACK_TCP_STATUS_LISTEN;

#endif

int global_port_id=0;
static struct rte_eth_conf port_c={//port_c配置参数对象，专门告诉 DPDK“这张网卡我要怎么配置”，网卡配置单
    .rxmode={
        .max_rx_pkt_len=RTE_ETHER_MAX_LEN, //允许网卡接收的单个以太网帧最大长度按标准帧长度配置，大约 1518 字节
    },
};
/*
rte_eth_conf结构体是通过程序告诉dpdk网卡的配置参数，我要怎么配置这张网卡
*/

/*
nb_sys_ports为dpdk绑定的网卡的数量，后续的数据就从这个网卡接收
*/

/*
ustack_init_port()函数对网卡进行初始化
*/
static int ustack_init_port(struct rte_mempool *mbuf_pool){
    uint16_t nb_sys_ports = rte_eth_dev_count_avail();//返回当前 DPDK 可以使用的 Ethernet 设备数量
    //后面所有操作都建立在一个前提上：至少存在一张 DPDK 可以使用的网卡
    if(nb_sys_ports==0){
        rte_exit(EXIT_FAILURE,"No supported eth found\n");
        printf("nb_sys_ports:%d\n",nb_sys_ports);
    }
    struct rte_eth_dev_info dev_info;
    /*
    rte_eth_dev_info结构体是通过dpdk获取网卡本身的信息
    */
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
    //ethhdr header
    struct rte_ether_hdr *ethhdr = (struct rte_ether_hdr *)msg;
    rte_memcpy(ethhdr->s_addr.addr_bytes, smac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(ethhdr->d_addr.addr_bytes, dmac, RTE_ETHER_ADDR_LEN);
    ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);//告诉下一层这个以太网帧后面装的是 IPv4 数据包
    //ip header
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(ethhdr + 1); //msg + sizeof(struct rte_ether_hdr);
    //ethhdr + 1=ethhdr的地址 + sizeof(struct rte_ether_hdr)
	ip->version_ihl = 0x45;
	ip->type_of_service = 0;
	ip->total_length = htons(tol_len - sizeof(struct rte_ether_hdr));
	ip->packet_id = 0;
	ip->fragment_offset = 0;
	ip->time_to_live = 64;
    ip->dst_addr=dip;
    ip->src_addr=sip;
    ip->next_proto_id = IPPROTO_UDP;
    ip->hdr_checksum = 0;
    ip->hdr_checksum=rte_hdr_cksum(ip);
    //UDP header
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(ip + 1);
    udp->src_port=sport;
    udp->dst_port=dport;
    udp->dgram_len=htons(tol_len - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr));
    udp->dgram_cksum=0;
    udp->dgram_cksum=rte_ipv4_udptcp_cksum(ip,udp);
    rte_memcpy((uint8_t)(udp+1),data,udp->dgram_len);
}

static int ustack_encode_tcp_pkt(uint8_t*msg,uint16_t tol_len){
    //ethhdr header
    struct rte_ether_hdr *ethhdr = (struct rte_ether_hdr *)msg;
    rte_memcpy(ethhdr->s_addr.addr_bytes, smac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(ethhdr->d_addr.addr_bytes, dmac, RTE_ETHER_ADDR_LEN);
    ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    //ip header
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(ethhdr + 1); //msg + sizeof(struct rte_ether_hdr);
    //ethhdr + 1=ethhdr的地址 + sizeof(struct rte_ether_hdr)
	ip->version_ihl = 0x45;
	ip->type_of_service = 0;
	ip->total_length = htons(tol_len - sizeof(struct rte_ether_hdr));
	ip->packet_id = 0;
	ip->fragment_offset = 0;
	ip->time_to_live = 64;
    ip->dst_addr=dip;
    ip->src_addr=sip;
    ip->next_proto_id = IPPROTO_TCP;
    ip->hdr_checksum = 0;
    ip->hdr_checksum=rte_hdr_cksum(ip);
    //TCP header
    struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(ip + 1);
    memset(tcp, 0, sizeof(struct rte_tcp_hdr));
    tcp->src_port=sport;
    tcp->dst_port=dport;
    tcp->sent_seq=htonl(12345);
    tcp->recv_ack=htonl(seqnum+1);
    tcp->data_off=0x50;
    tcp->tcp_flags=RTE_TCP_SYN_FLAG|RTE_TCP_ACK_FLAG;
    tcp->cksum=0;
    tcp->cksum=rte_ipv4_udptcp_cksum(ip,tcp);
    tcp->tcp_urp=0;
}

#if ENABLE_ARP
static int ustack_encode_arp_pkt(uint8_t*msg,uint16_t opcode,uint8_t*dmac,uint32_t sip,uint32_t dip){
    //ethhdr header
    struct rte_ether_hdr *ethhdr = (struct rte_ether_hdr *)msg;
    rte_memcpy(ethhdr->s_addr.addr_bytes, smac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(ethhdr->d_addr.addr_bytes, dmac, RTE_ETHER_ADDR_LEN);
    ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP);
    //ARP header
    struct rte_arp_hdr *arphdr = (struct rte_arp_hdr *)(ethhdr + 1);
    arphdr->arp_hrd = rte_cpu_to_be_16(RTE_ARP_HRD_ETHER);
    arphdr->arp_pro = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    arphdr->arp_hln = RTE_ETHER_ADDR_LEN;
    arphdr->arp_pln = sizeof(uint32_t);
    arphdr->arp_op = rte_cpu_to_be_16(opcode);
    rte_memcpy(arphdr->arp_data.arp_sha.addr_bytes, smac, RTE_ETHER_ADDR_LEN);
    arphdr->arp_data.arp_sip = sip;
    rte_memcpy(arphdr->arp_data.arp_tha.addr_bytes, dmac, RTE_ETHER_ADDR_LEN);
    arphdr->arp_data.arp_tip = dip;
    return 0;
}

static struct rte_mbuf*ng_send_arp(struct rte_mempool *mbuf_pool,uint16_t opcode,uint8_t*smac,uint8_t*dmac,uint32_t sip,uint32_t dip){
    struct rte_mbuf*m=rte_pktmbuf_alloc(mbuf_pool);
    if(m==NULL){
        rte_exit(EXIT_FAILURE,"Cannot create tx mbuf\n");
    }
    m->data_len=sizeof(struct rte_ether_hdr)+sizeof(struct rte_arp_hdr);
    m->pkt_len=sizeof(struct rte_ether_hdr)+sizeof(struct rte_arp_hdr);
    uint8_t*msg=rte_pktmbuf_mtod(m,uint8_t*);
    ustack_encode_arp_pkt(msg, opcode, smac, dmac, sip, dip);
    return m;
}
#endif

#if ENABLE_ICMP

static uint16_t ng_checksum(uint16_t *addr, int len) {
    int sum = 0;
    while (len > 1) {
        sum += *addr++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(uint8_t *)addr;
    }
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (uint16_t)(~sum);
}

static int ustack_encode_icmp_pkt(uint8_t*msg,uint8_t*dmac,uint32_t sip,uint32_t dip,uint16_t id,uint16_t seqnum){
    //ethhdr header
    struct rte_ether_hdr *ethhdr = (struct rte_ether_hdr *)msg;
    rte_memcpy(ethhdr->s_addr.addr_bytes, smac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(ethhdr->d_addr.addr_bytes, dmac, RTE_ETHER_ADDR_LEN);
    ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    //ip header
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(ethhdr + 1); //msg + sizeof(struct rte_ether_hdr);
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(sizeof(struct rte_ipv4_hdr)+sizeof(struct rte_icmp_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->dst_addr=dip;
    ip->src_addr=sip;
    ip->next_proto_id = IPPROTO_ICMP;
    ip->hdr_checksum = 0;
    ip->hdr_checksum=rte_hdr_cksum(ip);
    //ICMP header
    struct rte_icmp_hdr *icmphdr = ((struct rte_icmp_hdr *)msg + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
    icmphdr->icmp_type=RTE_IP_ICMP_ECHO_REPLY;
    icmphdr->icmp_code=0;
    icmphdr->icmp_ident=id;
    icmphdr->icmp_seq_nb=seqnum;
    icmphdr->icmp_cksum=0;
    icmphdr->icmp_cksum=ng_checksum((uint16_t *)icmphdr, sizeof(*icmphdr));
    return 0;
}

static struct rte_mbuf*ng_send_icmp(struct rte_mempool *mbuf_pool,uint8_t dmac,uint32_t dip,uint32_t sip,uint16_t id,uint16_t seqnum){
    struct rte_mbuf*m=rte_pktmbuf_alloc(mbuf_pool);
    if(m==NULL){
        rte_exit(EXIT_FAILURE,"Cannot create tx mbuf\n");
    }
    m->data_len=sizeof(struct rte_ether_hdr)+sizeof(struct rte_ipv4_hdr)+sizeof(struct rte_icmp_hdr);
    m->pkt_len=sizeof(struct rte_ether_hdr)+sizeof(struct rte_ipv4_hdr)+sizeof(struct rte_icmp_hdr);
    uint8_t*msg=rte_pktmbuf_mtod(m,uint8_t*);
    ustack_encode_icmp_pkt(msg, dmac, sip, dip,id,seqnum);
    return m;
}
#endif

#if ENABLE_TIMER
static void arp_request_timer(__attribute__((unused)) struct rte_timer *tim,
	   void *arg){
        struct rte_mempool*mbuf=(struct rte_mempool*)arg;
        struct inout_ring*ring=rinstance();
        #if 0
        ng_send_arp(mbuf,RTE_ARP_OP_REQUEST,RTE_ETHER_ADDR_LEN,ahdr->arp_data.arp_sha.addr_bytes,ahdr->arp_data.arp_sip,ahdr->arp_data.arp_tip);
        rte_eth_tx_burst(gDpdkPortId, 0, &arpbuf, 1);
	    rte_pktmbuf_free(arpbuf);
        #endif
        int i=0;
        for(i=1;i<254;i++){
            uint32_t dstip = (gLocalIp & 0x00FFFFFF) | (0xFF000000 & (i << 24));
            struct in_addr addr;
            addr.s_addr=dstip;
            struct rte_mbuf*arpbuf=NULL;
            uint8_t*mac_temp=arp_table_mac_lookup(dstip);
            if(mac_temp==NULL){
                arpbuf=ng_send_arp(mbuf,RTE_ARP_OP_REQUEST,gDefaultArpMac,dmac,sip,dstip);
            }
            else{
                arpbuf=ng_send_arp(mbuf,RTE_ARP_OP_REQUEST,mac_temp,dmac,sip,dstip);
            }
            //rte_eth_tx_burst(gDpdkPortId, 0, &arpbuf, 1);
		    //rte_pktmbuf_free(arpbuf);
            rte_ring_mp_enqueue_burst(ring->out, (void **)&arpbuf, 1, NULL);
        }
       }
#endif

#if ENABLE_UDP_APP

struct udp_offload{
    uint32_t sip;
    uint32_t dip;

    uint16_t sport;
    uint16_t dport;

    int protocol;

    unsigned char*data;
    uint16_t data_len;
}

static struct localhost* gethost_from_ip_port(uint32_t dip,uint16_t dport,uint8_t proto){
    struct localhost*host;
    for(host=lhost;host!=NULL;host=host->next){
        if(host->localip==dip&&host->localport==dport&&proto==host->protocol)
        return host;
    }
    return NULL;
}

 static int udp_process(struct rte_mbuf *mbufs){
        struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(mbufs, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
        struct rte_udp_hdr *udphdr = (struct rte_udp_hdr *)(iphdr + 1);

        struct localhost *host = get_hostinfo_fromip_port(iphdr->dst_addr, udphdr->dst_port, iphdr->next_proto_id);
            if (host == NULL) {
                rte_pktmbuf_free(udpmbuf);
                return -3;
            } 


        struct udp_offload*udp=malloc(sizeof(struct udp_offload));
            if(udp==NULL){
                rte_ptkmuf_free(mbufs);
                return -1;
            }
        udp->sip=iphdr->src_addr;
        udp->dip=iphdr->dst_addr;
        udp->sport=udphdr->src_port;
        udp->dport=udphdr->dst_port;
        udp->protocol=IPPROTO_UDP;

        udp->data_len=ntohs(udphdr->dgram_len);
        udp->data=rte_malloc("unsigned char*",ntohs(udphdr->dgram_len)-sizeof(struct rte_udp_hdr),0);
        rte_memcpy(udp->data,(uint8_t *)(udphdr + 1),ntohs(udphdr->dgram_len)-sizeof(struct rte_udp_hdr));

        rte_ring_mp_enqueue(host->rbuf, udp);

        pthread_mutex_lock(&host->mutex);
        pthread_cond_signal(&host->cond);
        pthread_mutex_unlock(&host->mutex);
        
        rte_pktmbuf_free(mbufs[i]);
    }
#endif


#if ENABLE_MULTHREAD
    static int pkt_process(__attribute__((unused)) void* arg){
        struct rte_mempool*mbuf_pool = (struct rte_mempool*)arg;
        struct inout_ring*ring=rinstance();
        while(1){
        struct rte_mbuf *bufs[BURST_SIZE];
        unsigned int nb_rx = rte_ring_mc_dequeue_burst(ring->in, (void **)bufs, BURST_SIZE, NULL);
        for(int i=0;i<nb_rx;i++){
            struct rte_ether_hdr *ethhdr = rte_pktmbuf_mtod(bufs[i], struct rte_ether_hdr *);
            #if ENABLE_ARP
            if(ethhdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP)) {
                struct rte_arp_hdr *arphdr = rte_pktmbuf_mtod_offset(bufs[i], struct rte_arp_hdr *, sizeof(struct rte_ether_hdr));
                if(arphdr->arp.data.arp_tip == sip) {
                    if(arphdr->arp.opcode == rte_cpu_to_be_16(RTE_ARP_OP_REQUEST)) {
                        struct rte_mbuf*arpbuf=ng_send_arp(mbuf_pool, arphdr->arp.data.arp_sha.addr_bytes, arphdr->arp.data.arp_tha.addr_bytes, arphdr->arp.data.arp_dip, arphdr->arp.data.arp_sip);
                        rte_ring_mp_enqueue_burst(ring->out, (void **)&arpbuf, 1, NULL);
                        printf("Received ARP packet\n");
                        rte_pktmbuf_free(bufs[i]);
                    }
                    else if(arphdr->arp.opcode == rte_cpu_to_be_16(RTE_ARP_OP_REPLY)) {
                        struct arp_table*table=arp_table_instance();
                        uint8_t*mac=arp_table_mac_lookup(arphdr->arp.data.arp_sip);
                        if(mac==NULL){
                            arp_entry*entry=rte_malloc(NULL,sizeof(arp_entry),0);
                            if(entry){
                                entry->ip=arphdr->arp.data.arp_sip;
                                rte_memcpy(entry->mac,arphdr->arp.data.arp_sha.addr_bytes,RTE_ETHER_ADDR_LEN);
                                entry->status=ARP_ENTRY_STATUS_DYNAMIC;//动态记录通常表示：这条 IP 与 MAC 的对应关系是通过 ARP 通信自动学习得到的
                                LL_ADD(entry,table->entries);
                                table->count++;
                            }
                        }
                        printf("Received ARP reply\n");
                        rte_pktmbuf_free(bufs[i]);
                    }
                    continue;
                }
            }
            #endif
			if (ethhdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
				continue;
			}

			struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(bufs[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
			if (iphdr->next_proto_id == IPPROTO_UDP) {
                  udp_process(bufs[i]);
            }
            #if ENABLE_ICMP
            if(iphdr->next_proto_id == IPPROTO_ICMP) {
                struct rte_icmp_hdr *icmphdr = (struct rte_icmp_hdr *)(iphdr + 1);
                if(icmphdr->icmp_type==RTE_IP_ICMP_ECHO_REQUEST){
                    struct rte_mbuf*icmpbuf=ng_send_icmp(mbuf_pool, ethhdr->s_addr.addr_bytes, iphdr->src_addr, iphdr->dst_addr, icmphdr->icmp_ident, icmphdr->icmp_seq_nb);
                    //rte_eth_tx_burst(global_portid, 0, &icmpbuf, 1);
                    //rte_pktmbuf_free(icmpbuf);
                    rte_ring_mp_enqueue_burst(ring->out, (void **)&icmpbuf, 1, NULL);
                    rte_pktmbuf_free(bufs[i]);
                    printf("Received ICMP echo request\n");
                    // Handle ICMP echo request
                }
            }
        
            #endif
        
                else if(iphdr->next_proto_id == IPPROTO_TCP) {
                    tcp_process(bufs[i]);
                    #if 0
                    struct rte_tcp_hdr *tcphdr = (struct rte_tcp_hdr *)(iphdr + 1);
                    struct in_addr addr;
				addr.s_addr = iphdr->src_addr;
				printf("sip %s:%d --> ", inet_ntoa(addr), ntohs(tcphdr->src_port));
                //inet_ntoa() 把 IPv4 的二进制地址转换成 "192.168.1.10" 这样的字符串

				addr.s_addr = iphdr->dst_addr;
				printf("dip %s:%d --> ", inet_ntoa(addr), ntohs(tcphdr->dst_port));
                //ntohs() 把网络字节序的端口号转换成主机字节序的端口号

                rte_memcpy(smac, ethhdr->s_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
                rte_memcpy(dmac, ethhdr->d_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
                rte_memcpy(&sip, &iphdr->dst_addr, sizeof(uint32_t));
                rte_memcpy(&dip, &iphdr->src_addr, sizeof(uint32_t));
                rte_memcpy(&sport, &tcphdr->dst_port, sizeof(uint16_t));
                rte_memcpy(&dport, &tcphdr->src_port, sizeof(uint16_t));

                flags=tcphdr->tcp_flags;
                seqnum=ntohl(tcphdr->sent_seq);
                acknum=ntohl(tcphdr->recv_ack);
                
                if(tcp_flags&RTE_TCP_SYN_FLAG){
                    if(tcp_status==USTACK_TCP_STATUS_LISTEN){
                    
                uint16_t total_len=sizeof(struct rte_tcp_hdr)+sizeof(struct rte_ether_hdr)+sizeof(struct rte_ipv4_hdr);
                struct rte_mbuf*m=rte_ptkmbuf_alloc(mbuf_pool);
                if(m==NULL){
                    rte_exit(EXIT_FAILURE,"Cannot create tx mbuf\n");
                }
                m->data_len=total_len;//当前这个 mbuf 里实际有多少字节数据
                m->pkt_len=total_len;//整个 packet 一共有多少字节,由于这里是一个mbuf存一个packet，因此两者相等
                uint8_t*msg=rte_pktmbuf_mtod(m,uint8_t*);
                ustack_encode_tcp_pkt(msg,total_len);
                //rte_eth_tx_burst(global_portid, 0, &m, 1);
                rte_ring_mp_enqueue_burst(ring->out, (void **)&m, 1, NULL);
                tcp_status=USTACK_TCP_STATUS_SYN_RCVD;
                }
                }
                else if(tcp_flags&RTE_TCP_ACK_FLAG){
                    if(tcp_status==USTACK_TCP_STATUS_SYN_RCVD){
                        tcp_status=USTACK_TCP_STATUS_ESTABLISHED;
                        uint8_t hdrlen = (tcphdr->data_off >> 4) * sizeof(uint32_t);

						uint8_t *data = ((uint8_t*)tcphdr + hdrlen);

						printf("tcp data: %s\n", data);

                    }
                }
                #endif
            }

        }
        #if ENABLE_UDP_APP
        udp_out(mbuf_pool);
        #endif

        #if ENABLE_TCP_APP
        ng_tcp_out(mbuf_pool);
        #endif
    }
    return 0;
}
#endif

#if ENABLE_UDP_APP
#define DEFAULTY_FD 3
    static int get_fd_frombitmap(void) {

	int fd = DEFAULT_FD_NUM;
	for ( ;fd < MAX_FD_COUNT;fd ++) {
		if ((fd_table[fd/8] & (0x1 << (fd % 8))) == 0) {
			fd_table[fd/8] |= (0x1 << (fd % 8));
			return fd;
		}
	}

	return -1;
	
}

static int set_fd_frombitmap(int fd) {

	if (fd >= MAX_FD_COUNT) return -1;

	fd_table[fd/8] &= ~(0x1 << (fd % 8));

	return 0;
}


    static void* gethostinfo_from_fd(int fd){
        struct localhost*host;
        for(host=lhost;host!=NULL;host=host->next){
            if(host->fd==fd)
            return host;
        }
        struct ng_tcp_stream*stream;
        srtuct ng_tcp_table*table=ng_tcp_table_instance();
        for(stream=table->tcb_set;stream!=NULL;stream=stream->next){
            if(stream->fd==fd)
            return stream;
        }
        return NULL;
    }

    static int nsocket(int domain, int type, int protocol)
    {
        int fd = get_fd_from_bitmap();
        if(type==SOCK_DGRAM){
        struct localhost*host=rte_malloc("localhost",sizeof(struct localhost),0);
        memset(host,0,sizeof(struct localhost));
        if(host==NULL){
            return -1;
        }
        host->fd=fd;
        host->protocol=IPPROTO_UDP;

        host->rbuf=rte_ring_create("recv buffer",RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
        if(host->rbuf==NULL){
            rte_free(host);
            return -1;
        }
        host->sbuf=rte_ring_create("send buffer",RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
        if(host->sbuf==NULL){
            rte_free(host);
            return -1;
        }

        pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
        rte_memcpy(&host->cond, &blank_cond, sizeof(pthread_cond_t));

        pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
        rte_memcpy(&host->mutex, &blank_mutex, sizeof(pthread_mutex_t));
        LL_ADD(host,lhost);
        }

        else if(type==SOCK_STREAM){
            struct ng_tcp_stream*stream=rte_malloc("ng_tcp_stream",sizeof(struct ng_tcp_stream),0);
            memset(stream,0,sizeof(struct ng_tcp_stream));
            if(stream==NULL){
            return -1;
            }
            stream->fd=fd;
            stream->protocol=IPPROTO_TCP;
            stream->next = stream->prev = NULL;

            pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
            rte_memcpy(&stream->cond, &blank_cond, sizeof(pthread_cond_t));

            pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
            rte_memcpy(&stream->mutex, &blank_mutex, sizeof(pthread_mutex_t));

            struct ng_tcp_table*table=ng_tcp_table_instance();
            LL_ADD(stream,table->tcb_set);
        }
        return fd;
    }

    static int nbind(int sockfd,const struct sockaddr *addr,socklen_t addrlen){
        struct void*hostinfo = gethostinfo_from_fd(sockfd);
        if(hostinfo==NULL)
            return -1;
        struct localhost*host=(struct localhost*)hostinfo;
        if(host->protocol == IPPROTO_UDP){
            struct sockaddr_in*localaddr=(struct sockaddr_in*)addr;
            //struct sockaddr 没有 sin_port、`sin_addr` 这些 IPv4 专用成员
            //把通用地址指针 addr 转换成 IPv4 地址结构体指针，方便访问 IP 和端口

            host->localport=localaddr->sin_port;
            rte_memcpy(host->localmac,smac,RTE_ETHER_ADDR_LEN);
            rte_memcpy(host->localip,&localaddr->sin_addr.s_addr,sizeof(uint32_t));
        }
        else if(host->protocol == IPPROTO_TCP){
            struct ng_tcp_stream *stream = (struct ng_tcp_stream *)hostinfo;
		
            const struct sockaddr_in *laddr = (const struct sockaddr_in *)addr;
            stream->dport = laddr->sin_port;
            rte_memcpy(&stream->dip, &laddr->sin_addr.s_addr, sizeof(uint32_t));
            rte_memcpy(stream->localmac, gSrcMac, RTE_ETHER_ADDR_LEN);

            stream->status = NG_TCP_STATUS_CLOSED;

        }
        return 0;
    }

    static int nlisten(int sockfd, __attribute__((unused)) int backlog) { //

	void *hostinfo =  get_hostinfo_fromfd(sockfd);
	if (hostinfo == NULL) return -1;

	
	struct ng_tcp_stream *stream = (struct ng_tcp_stream *)hostinfo;
	if (stream->protocol == IPPROTO_TCP) {
		stream->status = NG_TCP_STATUS_LISTEN;
	}

	return 0;
}

    static int nclose(int sockfd){
        void*host=gethostinfo_from_fd(sockfd);
        struct localhost*host=(struct localhost*)host;
        if(host==NULL){
            return -1;
        }
        if(host->protocol==IPPROTO_UDP){
        LL_REMOVE(host,lhost);
        if(host->rbuf)
            rte_ring_free(host->rbuf);
        if(host->sbuf)
            rte_ring_free(host->sbuf);  
        rte_free(host);
    }
struct ng_tcp_stream *stream=(struct ng_tcp_stream *)host;
    if(stream->protocol==IPPROTO_TCP){
        if (stream->status != NG_TCP_STATUS_LISTEN) {
			
			struct ng_tcp_fragment *fragment = rte_malloc("ng_tcp_fragment", sizeof(struct ng_tcp_fragment), 0);
			if (fragment == NULL) return -1;

			printf("nclose --> enter last ack\n");
			fragment->data = NULL;
			fragment->length = 0;
			fragment->sport = stream->dport;
			fragment->dport = stream->sport;

			fragment->seqnum = stream->snd_nxt;
			fragment->acknum = stream->rcv_nxt;

			fragment->tcp_flags = RTE_TCP_FIN_FLAG | RTE_TCP_ACK_FLAG;
			fragment->windows = TCP_INITIAL_WINDOW;
			fragment->hdrlen_off = 0x50;

			rte_ring_mp_enqueue(stream->sndbuf, fragment);
			stream->status = NG_TCP_STATUS_LAST_ACK;

			
			set_fd_frombitmap(fd);

		} else { // nsocket

			struct ng_tcp_table *table = tcpInstance();
			LL_REMOVE(stream, table->tcb_set);	

			rte_free(stream);

		}

    }
        return 0;
    }

static ssize_t nrecvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
        struct localhost*host=gethostinfo_from_fd(sockfd);
        if(host==NULL)
        return -1;

        struct udp_offload*udp=NULL;
        unsigned char *ptr=NULL;

        struct sockaddr_in*addr=(struct sockaddr_in*)src_addr;
        int nb=-1;
        pthread_mutex_lock(&host->mutex);
        while((nb=rte_ring_mc_dequeue(host->rcvbuf, (void **)&ol))<0){
            pthread_cond_wait(&host->cond,&host->mutex);
        }
        pthread_mutex_unlock(&host->mutex);
        
        addr.sin_port=udp->sport;
        rte_memcpy(&addr.sin_addr.s_addr,&udp->sip,sizeof(uint32_t));

        if(udp->data_len>len){
        rte_memcpy(buf,udp->data,len);
        ptr=rte_malloc("unsigned char *",udp->data_len-len,0);
        rte_memcpy(ptr,udp->data+len,udp->data_len-len);

        rte_free(udp->data);
        udp->data=ptr;
        udp->data_len=udp->data_len-len;
        rte_ring_mp_enqueue(host->rbuf,udp);
        return len;
        }
        else{
            rte_memcpy(buf,udp->data,udp->data_len);
            rte_free(udp->data);
            rte-free(udp);

            return udp->data_len;
        }
    }

static ssize_t nsendto(int sockfd, const void *buf, size_t len,__attribute__((unused)) int flags,
const struct sockaddr *dest_addr,__attribute__((unused)) socklen_t addrlen) {
        struct localhost*host=gethostinfo_from_fd(sockfd);
        if(host==NULL)
        return -1;

        struct sockaddr_in*addr=(struct sockaddr_in*)src_addr;
        struct udp_offload *udp=rte_malloc(rte_malloc("offload", sizeof(struct offload), 0));

        udp->sip=host->localip;
        udp->dip=addr->sin_addr.s_addr;
        udp->sport=host->localport;
        udp->dport=addr->sin_port;
        udp->data_len=len;

        struct in_addr addr;
        addr.s_addr = udp->dip;

        udp->data=rte_malloc("unsigned char *", len, 0);
        if(udp->data==NULL){
            rte_free(udp);
            return -2;
        }

        rte_memcpy(udp->data,buf,len);
        rte_ring_mp_enqueue(host->sbuf, udp);
        return len;
    }

static int ng_encode_udp_apppkt(uint8_t *msg, uint32_t sip, uint32_t dip,
uint16_t sport, uint16_t dport, uint8_t *smac, uint8_t *dmac,
unsigned char *data, uint16_t total_len){
    //ethhdr header
    struct rte_ether_hdr *ethhdr = (struct rte_ether_hdr *)msg;
    rte_memcpy(ethhdr->s_addr.addr_bytes, smac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(ethhdr->d_addr.addr_bytes, dmac, RTE_ETHER_ADDR_LEN);
    ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);//告诉下一层这个以太网帧后面装的是 IPv4 数据包
    //ip header
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(ethhdr + 1); //msg + sizeof(struct rte_ether_hdr);
    //ethhdr + 1=ethhdr的地址 + sizeof(struct rte_ether_hdr)
	ip->version_ihl = 0x45;
	ip->type_of_service = 0;
	ip->total_length = htons(tol_len - sizeof(struct rte_ether_hdr));
	ip->packet_id = 0;
	ip->fragment_offset = 0;
	ip->time_to_live = 64;
    ip->dst_addr=dip;
    ip->src_addr=sip;
    ip->next_proto_id = IPPROTO_UDP;
    ip->hdr_checksum = 0;
    ip->hdr_checksum=rte_hdr_cksum(ip);
    //UDP header
    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(ip + 1);
    udp->src_port=sport;
    udp->dst_port=dport;
    udp->dgram_len=htons(tol_len - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr));
    udp->dgram_cksum=0;
    udp->dgram_cksum=rte_ipv4_udptcp_cksum(ip,udp);
    rte_memcpy((uint8_t)(udp+1),data,udp->dgram_len);
    return 0;
}


static struct rte_mbuf * ng_udp_pkt(struct rte_mempool *mbuf_pool, uint32_t sip, uint32_t dip,
	uint16_t sport, uint16_t dport, uint8_t *srcmac, uint8_t *dstmac,
	uint8_t *data, uint16_t length) {

	// mempool --> mbuf

	const unsigned total_len = length + 42;

	struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
	if (!mbuf) {
		rte_exit(EXIT_FAILURE, "rte_pktmbuf_alloc\n");
	}
	mbuf->pkt_len = total_len;
	mbuf->data_len = total_len;

	uint8_t *pktdata = rte_pktmbuf_mtod(mbuf, uint8_t*);

	ng_encode_udp_apppkt(pktdata, sip, dip, sport, dport, srcmac, dstmac,
		data, total_len);

	return mbuf;

}

static int udp_out(struct rte_mempool*mbuf){
        struct localhost*host;
        for(host=lhost;host!=NULL;host=host->next){
            struct offload *ol;
            int nb_snd = rte_ring_mc_dequeue(host->sndbuf, (void **)&ol);
            if (nb_snd < 0) continue;

            uint8_t macaddr=arp_table_mac_lookup();

            if(macaddr == NULL){
               struct rte_mbuf*arpbuf = ng_send_arp(mbuf, RTE_ARP_OP_REQUEST, gDefaultArpMac, 
				ol->sip, ol->dip);

                struct inout_ring *ring = ringInstance();
                rte_ring_mp_enqueue_burst(ring->out, (void **)&arpbuf, 1, NULL);
                rte_ring_mp_enqueue(host->sndbuf, ol);
            }
            else{
                struct rte_mbuf *udpbuf = ng_udp_pkt(mbuf, udp->sip, udp->dip, udp->sport, udp->dport,
				host->localmac, dstmac, udp->data, udp->length);

			struct inout_ring *ring = ringInstance();
			rte_ring_mp_enqueue_burst(ring->out, (void **)&udpbuf, 1, NULL);
            }
        }
    }

    int udp_server_entry(int argc,char*argv[]){
        int connfd=nsocket(AF_INET,SOCK_DGRAM,0);
        if(connfd<0){
            perror("socket error");
            return -1;
        }
        struct sockaddr_in serveraddr;
        memset(&serveraddr,0,sizeof(serveraddr));
        serveraddr.sin_family=AF_INET;
        serveraddr.sin_port=htons(8889);
        serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
        if(nbind(connfd,(struct sockaddr*)&serveraddr,sizeof(struct sockaddr_in))<0){
            perror("bind error");
            return -1;
        }
        char buffer[UDP_APP_SIZE]={0};
        while(1){
            struct sockaddr_in clientaddr;
            socklen_t clientaddr_len=sizeof(clientaddr);
            if(nrecvfrom(connfd,buffer,UDP_APP_SIZE,0,(struct sockaddr*)&clientaddr,&clientaddr_len)<0){
                continue;
            }
            else{
                nsendto(connfd,buffer,strlen(buffer),0,(struct sockaddr*)&clientaddr,clientaddr_len);
            }
            nclose(connfd);
        }
    }

#endif

#if ENABLE_TCP_APP

#define TCP_OPTION_LENGTH 8
#define TCP_MAX_SEQ 4294967295
#define TCP_INITIAL_WINDOW  14600

typedef enum _NG_TCP_STATUS {

	NG_TCP_STATUS_CLOSED = 0,
	NG_TCP_STATUS_LISTEN,
	NG_TCP_STATUS_SYN_RCVD,
	NG_TCP_STATUS_SYN_SENT,
	NG_TCP_STATUS_ESTABLISHED,

	NG_TCP_STATUS_FIN_WAIT_1,
	NG_TCP_STATUS_FIN_WAIT_2,
	NG_TCP_STATUS_CLOSING,
	NG_TCP_STATUS_TIME_WAIT,

	NG_TCP_STATUS_CLOSE_WAIT,
	NG_TCP_STATUS_LAST_ACK

} NG_TCP_STATUS;

struct ng_tcp_stream{
    int fd;

    uint32_t sip;
    uint32_t dip;

    uint16_t sport;
    uint16_t dport;

    int protocol;

    uint8_t localmac[RTE_ETHER_ADDR_LEN];

    uint32_t snd_nxt; // seqnum 本次从哪个序号开始发送。以服务器为例，
    //本次服务器期望发送的序号就是上次客户端发包中客户端期望从哪里开始接收的序号
	uint32_t rcv_nxt; // acknum 希望下次从哪里开始接受

	NG_TCP_STATUS status;

	struct rte_ring *sndbuf;
	struct rte_ring *rcvbuf;

	struct ng_tcp_stream *prev;
	struct ng_tcp_stream *next;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
}

struct ng_tcp_table{
    struct ng_tcp_stream*tcb_set;
    int count;
}

struct ng_tcp_table*tcp_table=NULL;
static ng_tcp_table *tcpInstance(void){
    if(tcp_table==NULL){
        tcp_table=rte_malloc("ng_tcp_table",sizeof(struct ng_tcp_table),0);
        memset(tcp_table,0,sizeof(struct ng_tcp_table));
    }
    return tcp_table;
}

struct ng_tcp_fragment{
    uint16_t sport;  
	uint16_t dport;  
	uint32_t seqnum;  
	uint32_t acknum;  
	uint8_t  hdrlen_off;  
	uint8_t  tcp_flags; 
	uint16_t windows;   
	uint16_t cksum;     
	uint16_t tcp_urp;  

	int optlen;
	uint32_t option[TCP_OPTION_LENGTH];

	unsigned char *data;
	int length;
}
//UDP socket 通常只绑定本地地址，可以和多个对端通信；TCP 则需要为每条已建立的连接记录固定的双方地址。

static struct ng_tcp_stream* ng_tcp_stream_search(uint32_t sip,uint32_t dip,uint16_t sport,uint16_t dport){
    struct ng_tcp_stream* item;
    static ng_tcp_table *table=tcpInstance();
    for(item=table->tcb_set;item!=NULL;item=item->next){
        if(item->sip==sip&&item->dip==dip&&item->sport==sport&&item->dport==dport){
            return item;
        }
    }

    for(item=table->tcb_set;item!=NULL;item=item->next){
        if(item->dport==dport && item->status==NG_TCP_STATUS_LISTEN){
            return item;
        }
    }

    return NULL;
}

static struct ng_tcp_stream* ng_tcp_stream_create(uint32_t sip,uint32_t dip,uint16_t sport,uint16_t dport){
    struct ng_tcp_stream*stream=rte_malloc("ng_tcp_stream",sizeof(struct ng_tcp_stream),0);
    if(stream==NULL)
    return NULL;
    memset(stream,0,sizeof(struct ng_tcp_stream));
    
    stream->sip=sip;
    stream->dip=dip;
    stream->sport=sport;
    stream->dport=dport;
    stream->protocol=IPPROTO_TCP;
    stream->status=NG_TCP_STATUS_LISTEN;
    stream->sndbuf=rte_ring_create("sndbuf",RING_SIZE,rte_socket_id(),0);
    stream->rcvbuf=rte_ring_create("rcvbuf",RING_SIZE,rte_socket_id(),0);
    stream->fd=-1;

    rte_memcpy(stream->localmac,smac,RTE_ETHER_ADDR_LEN);

    uint32_t next_seed = time(NULL);
	stream->snd_nxt = rand_r(&next_seed) % TCP_MAX_SEQ;
    
    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
	rte_memcpy(&stream->cond, &blank_cond, sizeof(pthread_cond_t));

	pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
	rte_memcpy(&stream->mutex, &blank_mutex, sizeof(pthread_mutex_t));

    //struct ng_tcp_table *table = tcpInstance();
	//LL_ADD(stream, table->tcb_set);

    return stream;
}

static int ng_tcp_handle_listen(struct ng_tcp_stream *stream, struct rte_tcp_hdr *tcphdr, struct rte_ipv4_hdr *iphdr) {
    if (tcphdr->tcp_flags & RTE_TCP_SYN_FLAG) {
        if(stream->status == NG_TCP_STATUS_LISTEN){

            struct ng_tcp_stream *syn = ng_tcp_stream_create(iphdr->dst_addr, iphdr->src_addr, tcphdr->dst_port, tcphdr->src_port);
            if(syn==NULL)
                return -1;
            struct ng_tcp_fragment *fragment = rte_malloc("ng_tcp_fragment", sizeof(struct ng_tcp_fragment), 0);
            if(fragment==NULL)
            return -2;
        memset(fragment,0,sizeof(struct ng_tcp_fragment));

        fragment->sport = tcphdr->dst_port;
        fragment->dport = tcphdr->src_port;
        fragment->seqnum = stream->snd_nxt;
        fragment->acknum = ntohl(tcphdr->sent_seq) + 1;
        syn->rcv_nxt = fragment->acknum;
        fragment->tcp_flags = RTE_TCP_SYN_FLAG | RTE_TCP_ACK_FLAG;
        fragment->windows = TCP_INITIAL_WINDOW;
        fragment->hdrlen_off = 0x50;
        fragment->data=NULL;

        rte_ring_mp_enqueue(syn->sndbuf,fragment);

        syn->status = NG_TCP_STATUS_SYN_RCVD;

        return 0;
        }
    }   
}

static int ng_encode_tcp_apppkt(uint8_t*msg,uint32_t sip, uint32_t dip,
	uint16_t sport, uint16_t dport, uint8_t *srcmac, uint8_t *dstmac,
	unsigned char *data, uint16_t total_len){
    //ethhdr header
    struct rte_ether_hdr *ethhdr = (struct rte_ether_hdr *)msg;
    rte_memcpy(ethhdr->s_addr.addr_bytes, smac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(ethhdr->d_addr.addr_bytes, dmac, RTE_ETHER_ADDR_LEN);
    ethhdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    //ip header
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(ethhdr + 1); //msg + sizeof(struct rte_ether_hdr);
    //ethhdr + 1=ethhdr的地址 + sizeof(struct rte_ether_hdr)
	ip->version_ihl = 0x45;
	ip->type_of_service = 0;
	ip->total_length = htons(tol_len - sizeof(struct rte_ether_hdr));
	ip->packet_id = 0;
	ip->fragment_offset = 0;
	ip->time_to_live = 64;
    ip->dst_addr=dip;
    ip->src_addr=sip;
    ip->next_proto_id = IPPROTO_TCP;
    ip->hdr_checksum = 0;
    ip->hdr_checksum=rte_hdr_cksum(ip);
    //TCP header
    struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr *)(ip + 1);
    memset(tcp, 0, sizeof(struct rte_tcp_hdr));
    tcp->src_port=fragment->sport;
    tcp->dst_port=fragment->dport;
    tcp->sent_seq=htonl(fragment->seqnum);
    tcp->recv_ack=htonl(fragment->acknum);
    tcp->data_off=fragment->hdrlen_off;
    tcp->tcp_flags=fragment->tcp_flags;
    tcp->tcp_urp=fragment->tcp_urp;
    tcp->rx_win = fragment->windows;

    if (fragment->data != NULL) {
            uint8_t *payload = (uint8_t*)(tcp+1) + fragment->optlen * sizeof(uint32_t);
            rte_memcpy(payload, fragment->data, fragment->length);
        }

    tcp->cksum=0;
    tcp->cksum=rte_ipv4_udptcp_cksum(ip,tcp);
}

static struct rte_mbuf * ng_tcp_pkt(struct rte_mempool *mbuf_pool, uint32_t sip, uint32_t dip,
	uint8_t *srcmac, uint8_t *dstmac,struct ng_tcp_fragment*fragment) {

	// mempool --> mbuf

	const unsigned total_len = length + 42;

	struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
	if (!mbuf) {
		rte_exit(EXIT_FAILURE, "rte_pktmbuf_alloc\n");
	}
	mbuf->pkt_len = total_len;
	mbuf->data_len = total_len;

	uint8_t *pktdata = rte_pktmbuf_mtod(mbuf, uint8_t*);

	ng_encode_tcp_apppkt(pktdata, sip, dip, sport, dport, srcmac, dstmac,
		data, total_len);

	return mbuf;

}

static int ng_tcp_handle_syn_rcvd(struct ng_tcp_stream *stream, struct rte_tcp_hdr *tcphdr){
    if(tcphdr->tcp_flags & RTE_TCP_ACK_FLAG){
        if(stream->status ==NG_TCP_STATUS_SYN_RCVD){
            uint32_t acknum = ntohl(tcphdr->recv_ack);
			if (acknum == stream->snd_nxt + 1) {
				// 
			}

        stream->status = NG_TCP_STATUS_ESTABLISHED;

        struct ng_tcp_stream *listener = ng_tcp_stream_search(0, 0, 0, stream->dport);
			if (listener == NULL) {
				rte_exit(EXIT_FAILURE, "ng_tcp_stream_search failed\n");
			}

			pthread_mutex_lock(&listener->mutex);
			pthread_cond_signal(&listener->cond);
			pthread_mutex_unlock(&listener->mutex);
        }
        

    }
    return 0;
}

struct ng_tcp_stream*get_apt_tcb(int dport){
    struct ng_tcp_stream*stream;
    static ng_tcp_table *table=tcpInstance();
    for(stream=table->tcb_set;stream!=NULL;stream=stream->next){
        if(stream->dport==dport && stream->status==NG_TCP_STATUS_ESTABLISHED && stream->fd==-1){
            return stream;
        }
    }
    return NULL;
}

static ssize_t ng_tcp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen) {
    struct ng_tcp_stream *stream = (struct ng_tcp_stream *)get_hostinfo_from_fd(sockfd);
    if (stream == NULL) return -1;

    if(stream->protocol == IPPROTO_TCP){
        struct ng_tcp_stream *apt=NULL;
        pthread_mutex_lock(&stream->mutex);
        while((apt=get_apt_tcb(stream->dport))==NULL){
            pthread_cond_wait(&stream->cond,&stream->mutex);
        }
        pthread_mutex_unlock(&stream->mutex);
        apt->fd = get_fd_frombitmap();
        struct sockaddr_in *client_addr = (struct sockaddr_in *)addr;
        client_addr->sin_port = apt->sport;
        rte_memcpy(&client_addr->sin_addr.s_addr, &apt->sip, sizeof(uint32_t));
        return apt->fd;
    }
    return -1;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    struct ng_tcp_stream *stream = (struct ng_tcp_stream *)get_hostinfo_from_fd(sockfd);
    if (stream == NULL) return -1;

    if(stream->protocol == IPPROTO_TCP){
        struct ng_tcp_fragment *fragment = rte_malloc("ng_tcp_fragment", sizeof(struct ng_tcp_fragment), 0);
        if(fragment==NULL)
            return -1;
        memset(fragment,0,sizeof(struct ng_tcp_fragment));
        fragment->sport = stream->sport;
        fragment->dport = stream->dport;
        fragment->seqnum = stream->snd_nxt;
        fragment->acknum = stream->rcv_nxt;
        fragment->tcp_flags = RTE_TCP_ACK_FLAG | RTE_TCP_PSH_FLAG;
        fragment->windows = TCP_INITIAL_WINDOW;
        fragment->hdrlen_off = 0x50;
        fragment->data=rte_malloc("unsigned char*",len+1,0);
        if(fragment->data==NULL){
            rte_free(fragment);
            return -2;
        }
        memset(fragment->data,0,len+1);
        rte_memcpy(fragment->data,buf,len);
        fragment->length=len;

        rte_ring_mp_enqueue(stream->sndbuf,fragment);

        return len;
    }
    return -1;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    struct ng_tcp_stream *stream = (struct ng_tcp_stream *)get_hostinfo_from_fd(sockfd);
    if (stream == NULL) return -1;

    if(stream->protocol == IPPROTO_TCP){
        struct ng_tcp_fragment *fragment=NULL;
        int nb=-1;
        pthread_mutex_lock(&stream->mutex);
        while((nb=rte_ring_mc_dequeue(stream->rcvbuf, (void **)&fragment))<0){
            pthread_cond_wait(&stream->cond,&stream->mutex);
        }
        pthread_mutex_unlock(&stream->mutex);

        if(fragment->length>len){
            rte_memcpy(buf,fragment->data,len);
            uint8_t *ptr=rte_malloc("unsigned char*",fragment->length-len,0);
            rte_memcpy(ptr,fragment->data+len,fragment->length-len);

            rte_free(fragment->data);
            fragment->data=ptr;
            fragment->length=fragment->length-len;
            rte_ring_mp_enqueue(stream->rcvbuf,fragment);
            return len;
        }
        else if(fragment->length==0){
            rte_free(fragment);
            return 0;
        }
        else{
            rte_memcpy(buf,fragment->data,fragment->length);
            rte_free(fragment->data);
            rte_free(fragment);
            return fragment->length;
        }
    }
    return -1;
}

static int ng_tcp_enqueue_recvbuffer(struct ng_tcp_stream *stream, struct rte_tcp_hdr *tcphdr, int tcplen){
    struct ng_tcp_fragment * rfragment = rte_malloc("ng_tcp_fragment", sizeof(struct ng_tcp_fragment), 0);
    if(rfragment==NULL)
    return -1;
    memset(rfragment,0,sizeof(struct ng_tcp_fragment));
    rfragment->sport = tcphdr->src_port;
    rfragment->dport = tcphdr->dst_port;
    uint8_t hdrlen = tcphdr->data_off >> 4;
    int payload_len = tcplen-hdrlen*4;
    if(payload_len>0){
        rfragment->data=rte_malloc("unsigned char*",payload_len+1,0);
        if(rfragment->data==NULL){
            rte_free(rfragment);
            return -2;
        }
        memset(rfragment->data,0,payload_len+1);
        uint8_t *payload = (uint8_t*)tcphdr+hdrlen*4;
        rte_memcpy(rfragment->data,payload,payload_len);
        rfragment->length=payload_len;
    }
    else if(payload_len == 0){
        rfragment->data=NULL;
        rfragment->length=0;
    }
    rte_ring_mp_enqueue(stream->rcvbuf,rfragment);
    return 0;
} 

static int ng_tcp_send_ackpkt(struct ng_tcp_stream *stream, struct rte_tcp_hdr *tcphdr){
    struct ng_tcp_fragment * ackfrag = rte_malloc("ng_tcp_fragment", sizeof(struct ng_tcp_fragment), 0);
    if(ackfrag==NULL){
        rte_free(rfragment);
        return -3;
    }
    memset(ackfrag, 0, sizeof(struct ng_tcp_fragment));
    ackfrag->dport = tcphdr->src_port;
	ackfrag->sport = tcphdr->dst_port;
    stream->rcv_nxt = stream->rcv_nxt + payloadlen;
    // local 
    stream->snd_nxt = ntohl(tcphdr->recv_ack);
    //ackfrag->

    ackfrag->acknum = stream->rcv_nxt;
    ackfrag->seqnum = stream->snd_nxt;

    ackfrag->tcp_flags = RTE_TCP_ACK_FLAG;
    ackfrag->windows = TCP_INITIAL_WINDOW;
    ackfrag->hdrlen_off = 0x50;
    ackfrag->data = NULL;
    ackfrag->length = 0;
    
    pthread_mutex_lock(&stream->mutex);

    int ret = rte_ring_mp_enqueue(stream->rcvbuf, rfragment);
    if (ret == 0) {
        pthread_cond_signal(&stream->cond);
    }

    pthread_mutex_unlock(&stream->mutex);

    if (ret != 0) {
        rte_free(rfragment->data);
        rte_free(rfragment);
        return -1;
    }
}

static int ng_tcp_handle_established(struct ng_tcp_stream *stream, struct rte_tcp_hdr *tcphdr, int tcplen) {
    if(tcphdr->tcp_flags & RTE_TCP_SYN_FLAG){

    }
    if (tcphdr->tcp_flags & RTE_TCP_PSH_FLAG){
    //recv buffer
    #if 0
    struct ng_tcp_fragment * rfragment = rte_malloc("ng_tcp_fragment", sizeof(struct ng_tcp_fragment), 0);
    if(rfragment==NULL)
    return -1;
    memset(rfragment,0,sizeof(struct ng_tcp_fragment));
    rfragment->sport = tcphdr->src_port;
    rfragment->dport = tcphdr->dst_port;
    uint8_t hdrlen = tcphdr->data_off >> 4;
    int payload_len = tcplen-hdrlen*4;
    if(payload_len>0){
        rfragment->data=rte_malloc("unsigned char*",payload_len+1,0);
        if(rfragment->data==NULL){
            rte_free(rfragment);
            return -2;
        }
        memset(rfragment->data,0,payload_len+1);
        uint8_t *payload = (uint8_t*)tcphdr+hdrlen*4;
        rte_memcpy(rfragment->data,payload,payload_len);
        rfragment->length=payload_len;
    }
    rte_ring_mp_enqueue(stream->rcvbuf,rfragment);
    #else if
    ng_tcp_enqueue_recvbuffer(stream, tcphdr, tcplen);
#endif

#if 0
    //ack ptk
    struct ng_tcp_fragment * ackfrag = rte_malloc("ng_tcp_fragment", sizeof(struct ng_tcp_fragment), 0);
    if(ackfrag==NULL){
        rte_free(rfragment);
        return -3;
    }
    memset(ackfrag, 0, sizeof(struct ng_tcp_fragment));
    ackfrag->dport = tcphdr->src_port;
	ackfrag->sport = tcphdr->dst_port;
    stream->rcv_nxt = stream->rcv_nxt + payloadlen;
    // local 
    stream->snd_nxt = ntohl(tcphdr->recv_ack);
    //ackfrag->

    ackfrag->acknum = stream->rcv_nxt;
    ackfrag->seqnum = stream->snd_nxt;

    ackfrag->tcp_flags = RTE_TCP_ACK_FLAG;
    ackfrag->windows = TCP_INITIAL_WINDOW;
    ackfrag->hdrlen_off = 0x50;
    ackfrag->data = NULL;
    ackfrag->length = 0;
    
    pthread_mutex_lock(&stream->mutex);

    int ret = rte_ring_mp_enqueue(stream->rcvbuf, rfragment);
    if (ret == 0) {
        pthread_cond_signal(&stream->cond);
    }

    pthread_mutex_unlock(&stream->mutex);

    if (ret != 0) {
        rte_free(rfragment->data);
        rte_free(rfragment);
        return -1;
    }
    #else if
      uint8_t hdrlen = tcphdr->data_off >> 4;
		int payloadlen = tcplen - hdrlen * 4;
		
		stream->rcv_nxt = stream->rcv_nxt + payloadlen;
		stream->snd_nxt = ntohl(tcphdr->recv_ack);

        ng_tcp_send_ackpkt(stream, tcphdr);
    #endif

    //echo ptk
    struct ng_tcp_fragment * echofrag = rte_malloc("ng_tcp_fragment", sizeof(struct ng_tcp_fragment), 0);
    if(echofrag==NULL){
        rte_free(echofrag);
        return -4;
    }
    memset(echofrag, 0, sizeof(struct ng_tcp_fragment));
    echofrag->dport = tcphdr->src_port;
	echofrag->sport = tcphdr->dst_port;
    stream->rcv_nxt = stream->rcv_nxt + payloadlen;
    stream->snd_nxt = ntohl(tcphdr->recv_ack);
    echofrag->acknum = stream->rcv_nxt;
    echofrag->seqnum = stream->snd_nxt;
    echofrag->tcp_flags = RTE_TCP_ACK_FLAG;
    echofrag->windows = TCP_INITIAL_WINDOW;
    echofrag->hdrlen_off = 0x50;
    
    echofrag->data=rte_malloc("unsigned char*",payload_len+1,0);
    if(echofrag->data==NULL){
        rte_free(echofrag);
        return -2;
    }
    memset(echofrag->data,0,payload_len+1);
    uint8_t *payload = (uint8_t*)tcphdr+hdrlen*4;
    rte_memcpy(echofrag->data,payload,payload_len);
    echofrag->length=payload_len;
    rte_ring_mp_enqueue(stream->sndbuf,echofrag);
    }

    if(tcphdr->tcp_flags & RTE_TCP_ACK_FLAG){

    }

    if(tcphdr->tcp_flags & RTE_TCP_FIN_FLAG){
       stream->status = NG_TCP_STATUS_CLOSE_WAIT;

        ng_tcp_enqueue_recvbuffer(stream, tcphdr, tcphdr->data_off >> 4);

        // send ack pkt
        stream->rcv_nxt = stream->rcv_nxt + 1;
        stream->snd_nxt = ntohl(tcphdr->recv_ack);

        ng_tcp_send_ackpkt(stream, tcphdr);
    }


    return 0;
}

static int ng_tcp_handle_close_wait(struct ng_tcp_stream *stream, struct rte_tcp_hdr *tcphdr){
    if(tcphdr->tcp_flags & RTE_TCP_FIN_FLAG){
        if(stream->status == NG_TCP_STATUS_CLOSE_WAIT){
            
        }
    }
   return 0;
}

static int ng_tcp_handle_last_ack(struct ng_tcp_stream *stream, struct rte_tcp_hdr *tcphdr){
    if(tcphdr->status == NG_TCP_STATUS_ACK_FLAG){
        if(stream->status == NG_TCP_STATUS_LAST_ACK){
            stream->status = NG_TCP_STATUS_CLOSED;
            struct ng_tcp_table *table = tcpInstance();
            LL_REMOVE(stream, table->tcb_set);
            rte_ring_free(stream->sndbuf);
			rte_ring_free(stream->rcvbuf);
            rte_free(stream);
        }
    }
    return 0;
}

static int ng_tcp_process(struct rte_mbuf *mbufs){
    struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(mbufs, struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));
    struct rte_tcp_hdr *tcphdr = (struct rte_tcp_hdr *)(iphdr + 1);

    //先检查 TCP 报文在传输中是否损坏，再处理连接状态和数据
    uint16_t tcp_cksum = tcphdr->cksum;
    tcphdr->cksum = 0;
    uint16_t cksum=rte_ipv4_udptcp_cksum(iphdr, tcphdr);
    if(cksum != tcp_cksum)
    return -1;

    struct tcp_stream* tcp_stream=ng_tcp_stream_search(iphdr->src_addr,iphdr->dst_addr,tcphdr->src_port,tcphdr->dst_port);
    if(tcp_stream==NULL){
        return -2;
    }

    switch (tcp_stream->status) {

		case NG_TCP_STATUS_CLOSED: //client 
			break;
			
		case NG_TCP_STATUS_LISTEN: // server
			ng_tcp_handle_listen(tcp_stream, tcphdr, iphdr);
			break;

		case NG_TCP_STATUS_SYN_RCVD: // server
			ng_tcp_handle_syn_rcvd(tcp_stream, tcphdr);
			break;

		case NG_TCP_STATUS_SYN_SENT: // client
			break;

		case NG_TCP_STATUS_ESTABLISHED: // server | client
            ng_tcp_handle_established(tcp_stream, tcphdr, iphdr);
            break;

        case NG_TCP_STATUS_FIN_WAIT_1: //  ~client
			break;
			
		case NG_TCP_STATUS_FIN_WAIT_2: // ~client
			break;
			
		case NG_TCP_STATUS_CLOSING: // ~client
			break;
			
		case NG_TCP_STATUS_TIME_WAIT: // ~client
			break;

		case NG_TCP_STATUS_CLOSE_WAIT: // ~server
            ng_tcp_handle_close_wait(tcp_stream, tcphdr);
			break;
			
		case NG_TCP_STATUS_LAST_ACK:  // ~server
            ng_tcp_handle_last_ack(tcp_stream, tcphdr);
			break;

	}

	return 0;

}
#endif

static int ng_tcp_out(struct rte_mempool*mbuf_pool){
    struct ng_tcp_table *table=tcpInstance();
    struct ng_tcp_stream *stream;
    for(stream=table->tcb_set;stream!=NULL;stream=stream->next){
        if(stream->sndbuf==NULL) continue;
        struct ng_tcp_fragment *fragment=NULL;
        int nb_snd = rte_ring_mc_dequeue(stream->sndbuf, (void**)&fragment);
        if(nb_snd<0) continue;

        uint8_t mac=arp_table_mac_lookup(stream->sip);
        if(mac==NULL){
            struct rte_mbuf*arpbuf = ng_send_arp(mbuf_pool, RTE_ARP_OP_REQUEST, gDefaultArpMac, 
            stream->dip, stream->sip);
            struct inout_ring *ring = ringInstance();
            rte_ring_mp_enqueue_burst(ring->out, (void **)&arpbuf, 1, NULL);
            rte_ring_mp_enqueue(stream->sndbuf, fragment);
        } 
        else{

        }
    }
}

int tcp_server_entry(int argc,char*argv[]){
    int listenfd=nsocket(AF_INET,SOCK_STREAM,0);
    if(listenfd<0)
    return -1;

    struct sockaddr_in serveraddr;
    memset(serveraddr,0,sizeof(serveraddr));
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_port=htons(9999);
    serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);

    bind()(listenfd,(struct sockaddr*)&serveraddr,sizeof(struct sockaddr_in));
    nlisten(listenfd,10);

    while(1){
        struct sockaddr_in clientaddr;
        socklen_t clientaddr_len=sizeof(clientaddr);
        int connfd=accept(listenfd,(struct sockaddr*)&clientaddr,&clientaddr_len);
        char buff[BUFFER_SIZE] = {0};
        while(1){
            int n = nrecvfrom(connfd, buff, BUFFER_SIZE, 0, (struct sockaddr*)&clientaddr, &clientaddr_len);
            if(n>0){
                nsend(connfd, buff, n, 0, (struct sockaddr*)&clientaddr, clientaddr_len);
            }
            else if(n==0){
                nclose(connfd);
            }
            else{
                perror("recv error");
                break;
            }
        }
        nclose(listenfd);
}

#if ENABLE_KNI_APP
static struct rte_kni* kni_alloc(struct rte_mempool *mbuf_pool){
    struct rte_kni*kni_handler=NULL;
    struct rte_kni_conf conf;
    memset(&conf,0,sizeof(conf));
    struct rte_kni_ops ops;
    memset(&ops,0,sizeof(ops));

    kni_handler=rte_kni_alloc(mbuf_pool,&conf,&ops);
}
#endif

int main(int argc,char *argv[])
{
    if(rte_eal_init(argc,argv)<0){//eal:环境抽象层，初始化DPDK环境，配置巨页，UIO/VFIO等参数
        rte_exit(EXIT_FAILURE,"Error with EAL initialization\n");
    }
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS, 0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if(mbuf_pool==NULL){
        rte_exit(EXIT_FAILURE,"Cannot create mbuf pool\n");
    }
    #if ENABLE_KNI_APP
    if(rte_kni_init(mbuf_pool) == -1){
        rte_exit(EXIT_FAILURE,"Cannot create kni\n");
    }
    #endif


    ustack_init_port(mbuf_pool);

    arp_table_mac_lookup(gLocalIp);

    #if ENABLE_TIMER
    rte_timer_subsystem_init();

	struct rte_timer arp_timer;
	rte_timer_init(&arp_timer);

	uint64_t hz = rte_get_timer_hz();
	unsigned lcore_id = rte_lcore_id();
	rte_timer_reset(&arp_timer, hz, PERIODICAL, lcore_id, arp_request_timer_cb, mbuf_pool);
    #endif

    #if ENABLE_RINGBUFFER
   static void inout_ring_init(void){
    struct inout_ring*ring=rinstance();
    if(ring->in==NULL){
        ring->in=rte_ring_create("in ring",RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
    }
    if(ring->out==NULL){
        ring->out=rte_ring_create("out ring",RING_SIZE,rte_socket_id(),RING_F_SP_ENQ|RING_F_SC_DEQ);
    }
    }
    #endif

    #if ENABLE_MULTHREAD
    lcore_id=rte_get_next_lcore(lcore_id, 1, 0);
    rte_eal_remote_launch(pkt_process, mbuf_pool, lcore_id);
    #endif

    #if ENABLE_UDP_APP
    lcore_id=rte_get_next_lcore(lcore_id, 1, 0);
    rte_eal_remote_launch(udp_server_entry, mbuf_pool, lcore_id);
    #endif

    while(1){
        struct rte_mbuf *rx[BURST_SIZE];//rx的销毁在用户态协议栈中在执行，生命周期延续到了协议栈
        const uint16_t nb_rx = rte_eth_rx_burst(global_port_id, 0, rx, BURST_SIZE);
        if(nb_rx>BURST_SIZE){
            rte_exit(EXIT_FAILURE,"error\n");
        }
        else if(nb_rx>0){
            rte_ring_sp_enqueue_burst(rinstance()->in, (void **)rx, nb_rx, NULL);
            printf("Received %u packets\n",nb_rx);

            struct rte_mbuf *tx[BURST_SIZE];
            const uint16_t nb_tx = rte_ring_sc_dequeue_burst(rinstance()->out, (void **)tx, BURST_SIZE, NULL);
            if(nb_tx>0){
                const uint16_t nb_tx_sent = rte_eth_tx_burst(global_port_id, 0, tx, nb_tx);

                for(int i=0;i<nb_tx;i++){
                    rte_pktmbuf_free(tx[i]);//tx的销毁在接收之后直接执行
                }
            }
            #if 0
           
           
        #endif
        //
    

    printf("hello dpdk\n");
    return 0;
     }
     #if ENABLE_TIMER
    static uint64_t prev_tsc = 0, cur_tsc;
    uint64_t diff_tsc;
    cur_tsc = rte_rdtsc();
    diff_tsc = cur_tsc - prev_tsc;
    if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
        rte_timer_manage();
        prev_tsc = cur_tsc;
    }
    #endif
    }
}