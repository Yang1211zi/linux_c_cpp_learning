#ifndef _ARP_H_
#define _ARP_H_

#include <rte_ether.h>


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