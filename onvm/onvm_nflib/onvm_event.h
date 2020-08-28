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

uint64_t EVENT_BITMASK;
struct rte_mempool *pubsub_msg_pool;
struct rte_mempool *event_msg_pool;
struct rte_mempool *event_send_msg_pool;

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

// TODO: Merge event_msg and event_send_msg into one struct. and call it struct pub_sub_msg
struct event_msg {
        int type; 
        void *data;
};

/* Sent to NF instead of pkts (Grace had the actual struct this is just a quick definition for testing purpouses */
struct event_send_msg {
        uint64_t event_id;
        void *pkt;      // TODO: rename to data
};

//Merge event_msg and event_send_msg into one struct
/*struct pub_sub_msg{
        uint32_t type;
        uint64_t event_id;
        void *data;
}*/

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
void publish_new_event(uint64_t event_id);
void publish_event(uint16_t dest_controller,uint64_t event_id);
void publish_event1(struct event_tree_node** events, struct event_tree_node *root, uint64_t event_id);

int init_pubsub_msg_pool(void);
void free_pubsub_msg_pool(void);
void pubsub_msg_pool_put(void *msg);
int lookup_pubsub_msg_pool(void);
void pubsub_msg_pool_store(void *pool);
void send_event_mempool(uint16_t dest_id);
void send_event_data_msg(uint64_t event_id, uint16_t dest_id, void *pkt);

int init_pubsub_event_msg_pool(void);
int init_pubsub_event_send_msg_pool(void);
void pubsub_event_msg_pool_put(void *msg);
void pubsub_event_send_msg_pool_put(void *msg);
void free_pubsub_event_msg_pool(void);
void free_pubsub_event_send_msg_pool(void);
void event_msg_pool_store(void *pool);
void event_send_msg_pool_store(void *pool);
void send_event_msg_pool(uint16_t dest_id);
void send_event_send_msg_pool(uint16_t dest_id);

/* For testing */
void print_targets(struct event_tree_node *event);

/*void init_pubsub_msg_pool(void){
        nf_msg_pool_pubsub = onvm_nflib_get_onvm_nf_msg_pool();
}*/

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
int lookup_pubsub_msg_pool(void){
        printf("init_pubsub_msg_pool+++++++++2\n");
        //struct rte_mempool *ret_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);
        pubsub_msg_pool = rte_mempool_lookup(PUBSUB_MSG_POOL_NAME);
        printf("init_pubsub_msg_pool+++++++++3\n");

        if(pubsub_msg_pool==NULL)
        {
                printf("pubsub_msg_pool is null\n");
        }

        return (pubsub_msg_pool == NULL); /* 0 on success */
}
void pubsub_msg_pool_store(void *pool){
        pubsub_msg_pool = pool;
}

#endif

#if 1
int init_pubsub_event_msg_pool(void){
        printf("Creating pubsub event msg pool '%s' ...\n", PUBSUB_EVENT_MSG_POOL_NAME);
        event_msg_pool = rte_mempool_create(PUBSUB_EVENT_MSG_POOL_NAME, MAX_NFS * PUBSUB_MSG_QUEUE_SIZE, PUBSUB_INFO_SIZE,
                                         PUBSUB_MSG_CACHE_SIZE, 0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

        return (event_msg_pool == NULL); /* 0 on success */
}
int init_pubsub_event_send_msg_pool(void){
        printf("Creating pubsub event send msg pool '%s' ...\n", PUBSUB_EVENT_SEND_MSG_POOL_NAME);
        event_send_msg_pool = rte_mempool_create(PUBSUB_EVENT_SEND_MSG_POOL_NAME, MAX_NFS * PUBSUB_MSG_QUEUE_SIZE, PUBSUB_INFO_SIZE,
                                         PUBSUB_MSG_CACHE_SIZE, 0, NULL, NULL, NULL, NULL, rte_socket_id(), NO_FLAGS);

        return (event_send_msg_pool == NULL); /* 0 on success */
}
void free_pubsub_event_msg_pool(void){
        printf("Freeing pubsub msg pool\n");
        rte_mempool_free(event_msg_pool);
}
void free_pubsub_event_send_msg_pool(void){
        printf("Freeing pubsub msg pool\n");
        rte_mempool_free(event_send_msg_pool);
}
#if 0
int lookup_pubsub_msg_pool(void){
        printf("init_pubsub_msg_pool+++++++++1\n");
        //struct rte_mempool *ret_msg_pool = rte_mempool_lookup(_NF_MSG_POOL_NAME);
        event_msg_pool = rte_mempool_lookup(PUBSUB_EVENT_MSG_POOL_NAME);
        printf("init_pubsub_msg_pool+++++++++2\n");
        event_send_msg_pool = rte_mempool_lookup(PUBSUB_EVENT_SEND_MSG_POOL_NAME);
        printf("init_pubsub_msg_pool+++++++++3\n");

        if((event_msg_pool==NULL) || (event_send_msg_pool==NULL))
        {
                printf("event_msg_pool or event_send_msg_pool is null\n");
        }

        return ((event_msg_pool == NULL)||(event_send_msg_pool == NULL)); /* 0 on success */
}
#endif

void event_msg_pool_store(void *pool){
        event_msg_pool = pool;
}
void event_send_msg_pool_store(void *pool){
        event_send_msg_pool = pool;
}
void pubsub_event_msg_pool_put(void *msg){
        if(event_msg_pool == NULL)
        {
                printf("event_msg_pool is NULL\n");
        }
        rte_mempool_put(event_msg_pool, (void*)msg);
}
void pubsub_event_send_msg_pool_put(void *msg){
        if(event_send_msg_pool == NULL)
        {
                printf("event_send_msg_pool is NULL\n");
        }
        rte_mempool_put(event_send_msg_pool, (void*)msg);
}
#endif
/*void pubsub_msg_pool_put(void *msg){
        rte_mempool_put(nf_msg_pool_pubsub, msg);
}*/

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

void
publish_new_event(uint64_t event_id) {
        struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        msg->type = PUBLISH;
        struct event_publish_data *data = rte_zmalloc("ev pub data", sizeof(struct event_publish_data), 0);
        data->event = gen_event_tree_node(event_id);
        msg->data = (void *)data;
        onvm_nflib_send_msg_to_nf(1, (void*)msg);
}

void
publish_event(uint16_t dest_controller,uint64_t event_id) {
        struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        msg->type = PUBLISH;
        struct event_publish_data *data = rte_zmalloc("ev pub data", sizeof(struct event_publish_data), 0);
        data->event = gen_event_tree_node(event_id);
        msg->data = (void *)data;
        onvm_nflib_send_msg_to_nf(dest_controller, (void*)msg);
}

void
publish_event1(struct event_tree_node** events, struct event_tree_node *root, uint64_t event_id){
        
        struct event_publish_data *data = rte_zmalloc("ev pub data", sizeof(struct event_publish_data), 0);
        
        data->event = gen_event_tree_node(event_id);
        add_event(root, data->event);
        events[data->event->event_id] = data->event;
}

void
test_sending_event(uint64_t event_id, uint16_t dest_id) {
        struct event_send_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_send_msg), 0);
        msg->event_id = event_id;
        onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
}

#if 0
//for nf_msg_pool_pubsub which get from onvm_nflib.c
int send_event_data(uint64_t event_id, uint16_t dest_id, void *pkt){
        
        //test the performance with no zmalloc
	struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        struct event_send_msg *msg_event = rte_zmalloc("ev msg", sizeof(struct event_send_msg), 0); 
        
        #if 0
        struct event_msg *msg;
        ret = rte_mempool_get(nf_msg_pool_pubsub, (void**)&msg);
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Unable to allocate event_msg msg from pool when trying to send msg to nf\n");
                return ret;
        }
        struct event_send_msg *msg_event;
        ret = rte_mempool_get(nf_msg_pool_pubsub, (void**)&msg_event);
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Unable to allocate event_send_msg msg from pool when trying to send msg to nf\n");
                return ret;
        }
        #endif

        // TODO: combine the above two structs and then use mempool_get here instead of zmalloc
        //       somewhere when the NF initializes it will have to create a new mempool - pubsub_msg_pool

        msg_event->event_id = event_id;
        msg_event->pkt = pkt;
	
        msg->type = SEND;
        msg->data = (void *)msg_event;

        //onvm_nflib_send_a_msg_to_nf(dest_id, (void*)msg);
        #if 1
        //send msgs one by one
        int ret = onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
        while (ret != 0)
        {
                ret = onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
        }
        #endif

        #if 0
        //send msgs with batch
        int ret = onvm_nflib_send_a_msg_to_nf(dest_id, (void*)msg);
        while (ret != 0)
        {
                ret = onvm_nflib_send_a_msg_to_nf(dest_id, (void*)msg);
        }
        #endif
        return 0;
}
#endif

#if 1 
void send_event_data_msg(uint64_t event_id, uint16_t dest_id, void *pkt){
        int ret;
        void *pkt_sent;
        //char pkt_tmp[1500];
        //memset(pkt_tmp, 0, 1500);

        ret = rte_mempool_get(pubsub_msg_pool, &pkt_sent);
        if (ret != 0) {
                RTE_LOG(INFO, APP, "Unable to allocate event_msg msg from pool when trying to send msg to nf\n");
                return;
        }
        
        printf("send_event_data_msg++++++++++++++1\n");
        rte_strlcpy((char*)pkt_sent, (char*)pkt, strlen((char*)pkt));
        printf("send_event_data_msg++++++++++++++2\n");
        
        send_event_data(event_id, dest_id, (void*)pkt_sent);
        
}
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
        msg->data = (void *)msg_event;
        printf("send_event_data+++++++++++++++will send a msg to nf\n");

        #if 1
        ret = onvm_nflib_send_a_msg_to_nf(dest_id, (void*)msg);
        while (ret != 0)
        {
                ret = onvm_nflib_send_a_msg_to_nf(dest_id, (void*)msg);
                //printf("onvm_event.h onvm_nflib_send_msg_to_nf\n");
                //exit(-1);
        }
        #endif
        #if 0
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
        printf("send_event_mempool+++++++++1\n");
        struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        msg->type = MEMPOOL;
        msg->data = (void*)pubsub_msg_pool;
        onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
}
void send_event_msg_pool(uint16_t dest_id){
        printf("send_event_mempool+++++++++1\n");
        struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        msg->type = MEMPOOL1;
        msg->data = (void*)event_msg_pool;
        onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
}
void send_event_send_msg_pool(uint16_t dest_id){
        printf("send_event_mempool+++++++++1\n");
        struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        msg->type = MEMPOOL2;
        msg->data = (void*)event_send_msg_pool;
        onvm_nflib_send_msg_to_nf(dest_id, (void*)msg);
}
#endif //by store pubsub_msg into pool.

#endif  // _ONVM_EVENT_H_
