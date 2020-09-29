/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
 *            2010-2019 Intel Corporation
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * The name of the author may not be used to endorse or promote
 *       products derived from this software without specific prior
 *       written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * onvm_common.h - shared data between host and NFs
 ********************************************************************/

#ifndef _ONVM_EVENT_H_
#define _ONVM_EVENT_H_

#include <rte_malloc.h>
#include <rte_memcpy.h>
//#include <netinet/ip.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include "onvm_common.h"

#define MAX_EVENTS 10000
#define MAX_FLOWS 64
#define MAX_SUBSCRIBERS 64
#define MAX_DEPTH 16 // 64/4

#define RETRIEVE 1
#define SUBSCRIBE 2
#define PUBLISH 3
#define SEND 4
#define MEMPOOL 5
#define MEMPOOL1 6
#define MEMPOOL2 7
#define GETMEMPOOL 8

uint64_t EVENT_BITMASK;
struct rte_mempool *pubsub_msg_pool;

/*****************EVENT IDS************************/
// 4 bits represent a prefix, 0 is reserved => 15 max prefix variations
#define ROOT_EVENT_ID 0 

#define PKT_EVENT_ID 0x1
#define PKT_TCP_EVENT_ID (PKT_EVENT_ID + (0x1<<4))
#define PKT_TCP_SYN_EVENT_ID (PKT_TCP_EVENT_ID + (0x1<<8))
#define PKT_TCP_FIN_EVENT_ID (PKT_TCP_EVENT_ID + (0x2<<8))
#define PKT_TCP_DPI_EVENT_ID (PKT_TCP_EVENT_ID + (0x3<<8))

#define FLOW_EVENT_ID 0x2
#define FLOW_TCP_EVENT_ID (FLOW_EVENT_ID + (0x1<<4))
#define FLOW_TCP_TERM_EVENT_ID (FLOW_TCP_EVENT_ID + (0x1<<8))
#define FLOW_NEW_EVENT_ID (FLOW_EVENT_ID + (0x2<<4))
#define FLOW_END_EVENT_ID (FLOW_EVENT_ID + (0x3<<4))
#define FLOW_LARGE_EVENT_ID (FLOW_EVENT_ID + (0x4<<4))

#define FLOW_TCP_SYN_EVENT_ID (FLOW_TCP_EVENT_ID + (0x2<<8))
#define FLOW_TCP_ESTABLISH_EVENT_ID (FLOW_TCP_EVENT_ID + (0x3<<8))
#define FLOW_TCP_END_EVENT_ID (FLOW_TCP_EVENT_ID + (0x4<<8))

#define STATS_EVENT_ID 0x3
#define FLOW_REQ_EVENT_ID 0x4
#define FLOW_DEST_EVENT_ID 0x5
#define DATA_RDY_EVENT_ID 0x6

#define PUBSUB_MSG_QUEUE_SIZE 4096
#define PUBSUB_INFO_SIZE (sizeof(struct event_msg)+sizeof(struct event_send_msg))
#define PUBSUB_MSG_CACHE_SIZE 32
#define NO_FLAGS 0

#define PUB_POOL 1
/****************************************************/

struct event_retrieve_data {
        struct event_tree_node *root;
        int done;
};

struct event_subscribe_data {
        int op; // subscribe or something else
        struct event_tree_node *event;
        uint16_t id;
        uint16_t flow_id;
};

struct event_publish_data {
        struct event_tree_node *event;
        int done;
};

struct nf_subscriber {
        uint16_t id;
        uint8_t flows[MAX_FLOWS];
};

struct event_tree_node {
        uint64_t event_id;
        uint16_t children_cnt;
        uint16_t subscriber_cnt;
        struct event_tree_node *parent;
        struct event_tree_node *children[MAX_EVENTS]; // sub events
        struct nf_subscriber *subscribers[MAX_SUBSCRIBERS];
};

struct event_tree {
        struct onvm_tree_node *children[MAX_EVENTS]; // sub events
}; 

// Use a union to hold whichever data type we need
// This might waste space if one type is much larger than the other
/*union event_data {
        struct event_send_msg send;
        struct event_publish_data publish;
        struct event_subscribe_data subscribe;
        struct event_retrieve_data retrieve;
};*/

/* Sent to NF instead of pkts (Grace had the actual struct this is just a quick definition for testing purpouses */
struct event_send_msg {
        uint64_t event_id;
        void *pkt;      // TODO: rename to data
};

struct event_init_data {
        int nf_id;
        struct rte_mempool* mempool;
};

// TODO: Merge event_msg and event_send_msg into one struct. and call it struct pub_sub_msg
struct event_msg{
        int type; 
        //void *data;
        //union event_data *event_data;
        union {
                struct event_send_msg send;
                struct event_publish_data *publish;
                struct event_subscribe_data *subscribe;
                struct event_retrieve_data *retrieve;
                void *msg_data;
        };
};
 
// PUT these structs in onvm_event_tcp.h  (and also make onvm_event_l3.h, onvm_event_ids.h)

// Happens once at the start of the 3way handshake
struct tcp_syn_event {
   int flow_id;  // how to define this? Use the sock id for flow_pub_mos
   struct sockaddr_in addrs[2]; /* Address of a 0: client and a 1: server */
   struct rte_mbuf *mbuf; // race condition between subscriber and DPDK sending out
      // either need to clone or increase reference counter on packet so DPDK
      // won't free it after sending out
      // Subscriber will need to free the clone or reduce ref cnt when it finishes
};

// Happens for every packet that arrives after 3way handshake
struct tcp_established_event {
   int flow_id;
   struct sockaddr_in addrs[2]; /* Address of a 0: client and a 1: server */
   int pkt_direction; /* 0: client->server, 1: server->client */
   int total_data_so_far;  // how much payload data has been sent in the TCP bytestream
   struct rte_mbuf *mbuf;
};

// happens once when connection ends
struct tcp_close_event {
   int flow_id;
   struct sockaddr_in addrs[2];   /* Address of a 0: client and a 1: server */
   int total_data; // does MOS track this?
   int total_time; // does MOS track this?
};

/* Public APIs */
struct event_tree_node *gen_event_tree_node(uint64_t event_id);
struct nf_subscriber *gen_nf_subscriber(void);
int add_event(struct event_tree_node *root, struct event_tree_node *child);
void subscribe_nf(struct event_tree_node *event, uint16_t id, uint16_t flow_id);
void subscribe_nf_noflow(struct event_tree_node *event, uint16_t nf_id);
int nf_subscribed_to_event(struct event_tree_node *root, uint64_t event_id, uint16_t nf_id, uint16_t flow_id);
struct event_tree_node *get_event(struct event_tree_node *root, uint64_t event_id);

void test_sending_event(uint64_t event_id, uint16_t dest_id);
int send_event_data(uint64_t event_id, uint16_t dest_id, void *pkt);
int add_event_node_child(struct event_tree_node *parent, struct event_tree_node *child);
int publish_event(uint16_t dest_controller,uint64_t event_id);

struct rte_mempool *get_pubsub_msg_pool(void);
int init_pubsub_msg_pool(void);
void free_pubsub_msg_pool(void);
void pubsub_msg_pool_put(void *msg);
struct rte_mempool* lookup_pubsub_msg_pool(void);
void pubsub_msg_pool_store(void *pool);
void send_event_mempool(uint16_t dest_id);

int get_ip_tcp_hdr(struct rte_mbuf* pkt, struct ipv4_hdr **iphdr, struct tcp_hdr **tcphdr);
void print_pkt(struct rte_mbuf* pkt);

#define uint32_t_to_char(ip, a, b, c, d) do {\
                *a = (unsigned char)(ip >> 24 & 0xff);\
                *b = (unsigned char)(ip >> 16 & 0xff);\
                *c = (unsigned char)(ip >> 8 & 0xff);\
                *d = (unsigned char)(ip & 0xff);\
        } while (0)

/* For testing */
void print_targets(struct event_tree_node *event);

#if 1
void print_pkt(struct rte_mbuf* pkt){
        printf("data1->pkt_len:%d\n",pkt->pkt_len);
        struct ether_hdr *eth = rte_pktmbuf_mtod(pkt, struct ether_hdr*);
        uint16_t type = rte_be_to_cpu_16(eth->ether_type);
        struct ipv4_hdr *ip = NULL;
        struct tcp_hdr *tcp = NULL;
        if(type == ETHER_TYPE_IPv4){
                ip = (struct ipv4_hdr *)(eth + 1);
                if(ip->next_proto_id == IPPROTO_TCP){
                        tcp = (struct tcp_hdr *)((unsigned char *)ip + sizeof(struct ipv4_hdr));
                }
        }
        //uint32_t total_length = rte_cpu_to_be_16(ip->total_length);//The total data length(not include mac);
        
        //uint8_t hdr_len = 14 + sizeof(struct ipv4_hdr) + (tcp->data_off>>4)*4;
        //printf("tcphdr_len16:%x, tcphdr_be16:%x, total hdr_len:%d\n",tcphdr_len16, tcphdr_be16, hdr_len);

        unsigned char a, b, c, d,m,n,p,q;
        uint32_t_to_char(rte_bswap32(ip->src_addr), &a, &b, &c, &d);
        printf("Packet Src:%hhu.%hhu.%hhu.%hhu ", a, b, c, d);
	uint32_t_to_char(rte_bswap32(ip->dst_addr), &m, &n, &p, &q);
	printf("Packet Dst:%hhu.%hhu.%hhu.%hhu ", m, n, p, q);
        printf("src port:%d, dst port:%d, tcp_flags:%x\n",rte_be_to_cpu_16(tcp->src_port),rte_be_to_cpu_16(tcp->dst_port),tcp->tcp_flags);
	//printf("\n");
}
#endif

#if 1
int init_pubsub_msg_pool(void){
        printf("Creating pubsub pool '%s' ...\n", PUBSUB_MSG_POOL_NAME);
        pubsub_msg_pool = rte_mempool_create(PUBSUB_MSG_POOL_NAME, MAX_NFS * PUBSUB_MSG_QUEUE_SIZE*2, PUBSUB_INFO_SIZE,
                                         PUBSUB_MSG_CACHE_SIZE, 0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

        return (pubsub_msg_pool == NULL); /* 0 on success */
        //return pubsub_msg_pool;
}
void free_pubsub_msg_pool(void){
        printf("Freeing pubsub msg pool\n");
        rte_mempool_free(pubsub_msg_pool);
}
void pubsub_msg_pool_put(void *msg){
        if(pubsub_msg_pool == NULL)
        {
                printf("pubsub_msg_pool is NULL\n");
                return;
        }
        rte_mempool_put(pubsub_msg_pool, (void*)msg);
}
struct rte_mempool *lookup_pubsub_msg_pool(void){
        struct rte_mempool *ret_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);
        pubsub_msg_pool = rte_mempool_lookup(PUBSUB_MSG_POOL_NAME);

        return ret_msg_pool; /* success or NULL */
}
void pubsub_msg_pool_store(void *pool){
        pubsub_msg_pool = pool;
}
struct rte_mempool *get_pubsub_msg_pool(void){
        struct rte_mempool *msg_pool = rte_mempool_lookup(PUBSUB_MSG_POOL_NAME);
        if (msg_pool==NULL){
                printf("pubsub_msg_pool is null\n");
        }
        return msg_pool;
}

#endif

struct event_tree_node *
gen_event_tree_node(uint64_t event_id) {
        struct event_tree_node *event;
        event = rte_zmalloc("event_node", sizeof(struct event_tree_node), 0);
	//event = (struct event_tree_node*)malloc(sizeof(struct event_tree_node)*1);
        event->event_id = event_id;

        return event;
}

struct nf_subscriber *
gen_nf_subscriber(void) {
        struct nf_subscriber *subscriber;
        subscriber = rte_zmalloc("nf_subscriber", sizeof(struct nf_subscriber), 0);
        return subscriber;
}

int
add_event_node_child(struct event_tree_node *parent, struct event_tree_node *child) {
        int i, j;

        /* TODO check prefix equality */

        for (i = 0; i < parent->children_cnt; i++) {
                /* Don't allow duplicate event IDs */
                if (parent->children[i]->event_id == child->event_id)
                        return -1;
        }
        parent->children[parent->children_cnt] = child;
        parent->children_cnt++;
	printf("add_event_node_child parent->event_id:%lu,children_cnt:%u\n",parent->event_id,parent->children_cnt-1);
        /* Copy all parent subscriptions to child */
        for (i = 0; i < parent->subscriber_cnt; i++) {
                for (j = 0; j < MAX_FLOWS; j++) {
                        if(parent->subscribers[i]->flows[j])
                                subscribe_nf(parent->children[i], parent->subscribers[i]->id, j);
                }
        }
        return 0;
}

int
add_event(struct event_tree_node *root, struct event_tree_node *child) {
        uint8_t i, prefix;
        struct event_tree_node *cur;

        if (child->event_id == 0) {
                return -1;
        }

        cur = root;

        for (i = 0; i < MAX_DEPTH; i++) {
                prefix = (child->event_id >> i*4) & 0xF;
                if (prefix == 0) {
                        printf("Prefixes must be non 0\n");
                        return -1;
                }
                /* if next prefix is 0, we've reached the end of the prefix chain*/
                if (((child->event_id >> ((i+1)*4)) & 0xF) == 0)
                        break;
                /* Substract 1 because prefixes are 1 indexed, arrays are 0 indexed */
                cur = cur->children[prefix - 1];
                if (cur == NULL) {
                        printf("Can't add event with id %" PRIu64", parent prefix at index %d doesn't exist", child->event_id, i);
                        return -1;
                }
        }
        return add_event_node_child(cur, child);
}

#if 1
void
subscribe_nf_noflow(struct event_tree_node *event, uint16_t nf_id) {
        int i;
        struct nf_subscriber *subscriber;

        subscriber = NULL;
        for (i = 0; i < event->subscriber_cnt; i++) {
                if (event->subscribers[i]->id == nf_id)
                        subscriber = event->subscribers[i];
        }

        if (subscriber == NULL) {
                subscriber = gen_nf_subscriber();
                subscriber->id = nf_id;
                event->subscribers[event->subscriber_cnt] = subscriber;
                event->subscriber_cnt++;
        }
        //subscriber->flows[flow_id] = 1;

        for (i = 0; i < event->children_cnt; i++) {
                subscribe_nf_noflow(event->children[i], nf_id);
        }
}
#endif

void
subscribe_nf(struct event_tree_node *event, uint16_t nf_id, uint16_t flow_id) {
        int i;
        struct nf_subscriber *subscriber;

        subscriber = NULL;
        for (i = 0; i < event->subscriber_cnt; i++) {
                if (event->subscribers[i]->id == nf_id)
                        subscriber = event->subscribers[i];
        }

        if (subscriber == NULL) {
                subscriber = gen_nf_subscriber();
                subscriber->id = nf_id;
                event->subscribers[event->subscriber_cnt] = subscriber;
                event->subscriber_cnt++;
        }
        subscriber->flows[flow_id] = 1;

        for (i = 0; i < event->children_cnt; i++) {
                subscribe_nf(event->children[i], nf_id, flow_id);
        }
}

struct event_tree_node *
get_event(struct event_tree_node *root, uint64_t event_id) {
        int i;
        struct event_tree_node *result;

        if (event_id == root->event_id)
                return root;

        /* Should do prefix search but the number of events is so small the performance diff is negligible */
        for (i = 0; i < root->children_cnt; i++) {
                result = get_event(root->children[i], event_id);
                if (result != NULL)
                        return result;
        }
        return NULL;
}

int
nf_subscribed_to_event(struct event_tree_node *root, uint64_t event_id, uint16_t nf_id, uint16_t flow_id) {
        int i;
        int subscribed = 0;
        struct event_tree_node *event;

        event = get_event(root, event_id);

        for (i = 0; i < event->subscriber_cnt; i++) {
                if (event->subscribers[i]->id == nf_id && event->subscribers[i]->flows[flow_id] == 1) {
                        subscribed = 1;
                        break;
                }
        }
        return subscribed;
}

void
print_targets(struct event_tree_node *event) {
        int i;
        printf("Send event %" PRIu64" to : ", event->event_id);
        for (i = 0; i < MAX_EVENTS; i++) {
                if (event->subscribers[i] != NULL)
                        printf("%d, ", event->subscribers[i]->id);
        }
        printf("\n");
}

#if 0
void
publish_new_event(uint64_t event_id) {
        struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        msg->type = PUBLISH;
        struct event_publish_data *data = rte_zmalloc("ev pub data", sizeof(struct event_publish_data), 0);
        data->event = gen_event_tree_node(event_id);
        msg->data = (void *)data;
        onvm_nflib_send_msg_to_nf(1, (void*)msg);
}
#endif

#if PUB_POOL
//pubsub_msg_pool
int
publish_event(uint16_t dest_controller,uint64_t event_id) {
        int ret;
        struct event_msg *msg;
        ret = rte_mempool_get(pubsub_msg_pool, (void**)&msg);
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Unable to allocate pubsub_msg_pool from pool when trying to send msg to nf\n");
                return ret;
        }
        msg->type = PUBLISH;
        struct event_publish_data *data = rte_zmalloc("ev pub data", sizeof(struct event_publish_data), 0);
        data->event = gen_event_tree_node(event_id);
        msg->publish = data;
        
        onvm_nflib_send_msg_to_nf(dest_controller, (void*)msg);
        return 0;
}
#else
//zmalloc
void
publish_event(uint16_t dest_controller,uint64_t event_id) {
        struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        msg->type = PUBLISH;
        struct event_publish_data *data = rte_zmalloc("ev pub data", sizeof(struct event_publish_data), 0);
        data->event = gen_event_tree_node(event_id);
        msg->data = (void *)data;
        onvm_nflib_send_msg_to_nf(dest_controller, (void*)msg);
}
#endif

#if 0
void
publish_event1(struct event_tree_node** events, struct event_tree_node *root, uint64_t event_id){
        
        struct event_publish_data *data = rte_zmalloc("ev pub data", sizeof(struct event_publish_data), 0);
        
        data->event = gen_event_tree_node(event_id);
        add_event(root, data->event);
        events[data->event->event_id] = data->event;
}
#endif

void
test_sending_event(uint64_t event_id, uint16_t dest_id) {
        struct event_send_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_send_msg), 0);
        msg->event_id = event_id;
        onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
}


#if 0
//for pubsub_msg_pool
int send_event_data(uint64_t event_id, uint16_t dest_id, void *pkt)
{
        int ret;
        struct event_msg *msg;
        
        ret = rte_mempool_get(pubsub_msg_pool, (void**)&msg);
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Unable to allocate pubsub_msg_pool from pool when trying to send msg to nf\n");
                return ret;
        }

        struct event_send_msg *msg_event;
        ret = rte_mempool_get(pubsub_msg_pool, (void**)&msg_event);
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Unable to allocate pubsub_msg_pool from pool when trying to send msg to nf\n");
                return ret;
        }

        msg_event->event_id = event_id;
        msg_event->pkt = pkt;
	
        msg->type = SEND;
        msg->send = *msg_event;
        //msg->data = (void *)msg_event;

        #if 1
        ret = onvm_nflib_send_a_msg_to_nf(dest_id, (void*)msg);
        while (ret != 0)
        {
                ret = onvm_nflib_send_a_msg_to_nf(dest_id, (void*)msg);
                //printf("onvm_event.h onvm_nflib_send_msg_to_nf\n");
                //exit(-1);
        }
        #else
        //send msgs one by one
        ret = onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
        while (ret != 0)
        {
                ret = onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
        }
        #endif
        return ret;

}
#endif
int send_event_data(uint64_t event_id, uint16_t dest_id, void *pkt)
{
        int ret;
        struct event_msg *msg;
        
        ret = rte_mempool_get(pubsub_msg_pool, (void**)&msg);
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Unable to allocate pubsub_msg_pool from pool when trying to send msg to nf\n");
                return ret;
        }
	
        msg->type = SEND;
        msg->send.event_id = event_id;
        msg->send.pkt = pkt;

        #if 0
        ret = onvm_nflib_send_a_msg_to_nf(dest_id, (void*)msg);
        while (ret != 0)
        {
                ret = onvm_nflib_send_a_msg_to_nf(dest_id, (void*)msg);
                //printf("onvm_event.h onvm_nflib_send_msg_to_nf\n");
                //exit(-1);
        }
        #else
        //send msgs one by one
        ret = onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
        while (ret != 0)
        {
                ret = onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
        }
        #endif
        return ret;

}
void send_event_mempool(uint16_t dest_id){
        struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        msg->type = MEMPOOL;
        msg->msg_data = (void*)pubsub_msg_pool;
        onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
}

int get_ip_tcp_hdr(struct rte_mbuf* pkt, struct ipv4_hdr **iphdr, struct tcp_hdr **tcphdr){
        //printf("data1->pkt_len:%d\n",pkt->pkt_len);
        struct ether_hdr *eth = rte_pktmbuf_mtod(pkt, struct ether_hdr*);
        uint16_t type = rte_be_to_cpu_16(eth->ether_type);
        struct ipv4_hdr *ip;
        struct tcp_hdr *tcp;
        if(type == ETHER_TYPE_IPv4){
                ip = (struct ipv4_hdr *)(eth + 1);
                if(ip->next_proto_id == IPPROTO_TCP){
                        tcp = (struct tcp_hdr *)((unsigned char *)ip + sizeof(struct ipv4_hdr));
                        *iphdr = ip;
                        *tcphdr = tcp;
                        return 0;  //success
                }
        }
        return -1;  //failed
}

#endif  // _ONVM_EVENT_H_
