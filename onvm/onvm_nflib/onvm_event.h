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

#include "onvm_common.h"

#define MAX_EVENTS 10000
#define MAX_FLOWS 64
#define MAX_SUBSCRIBERS 64
#define MAX_DEPTH 16 // 64/4

#define RETRIEVE 1
#define SUBSCRIBE 2
#define PUBLISH 3

uint64_t EVENT_BITMASK;

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

#define STATS_EVENT_ID 0x3

#define FLOW_REQ_EVENT_ID 0x4

#define FLOW_DEST_EVENT_ID 0x5

#define DATA_RDY_EVENT_ID 0x6
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

struct event_msg {
        int type; //retrieve or subscribe
        void *data;
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

/* Public APIs */
struct event_tree_node *gen_event_tree_node(uint64_t event_id);
struct nf_subscriber *gen_nf_subscriber(void);
int add_event(struct event_tree_node *root, struct event_tree_node *child);
void subscribe_nf(struct event_tree_node *event, uint16_t id, uint16_t flow_id);
int nf_subscribed_to_event(struct event_tree_node *root, uint64_t event_id, uint16_t nf_id, uint16_t flow_id);
struct event_tree_node *get_event(struct event_tree_node *root, uint64_t event_id);

int add_event_node_child(struct event_tree_node *parent, struct event_tree_node *child);
void publish_new_event(uint64_t event_id);

/* For testing */
void print_targets(struct event_tree_node *event);

struct event_tree_node *
gen_event_tree_node(uint64_t event_id) {
        struct event_tree_node *event;
        event = rte_zmalloc("event_node", sizeof(struct event_tree_node), 0);
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
        int i;

        /* TODO check prefix equality */

        for (i = 0; i < parent->children_cnt; i++) {
                /* Don't allow duplicate event IDs */
                if (parent->children[i]->event_id == child->event_id)
                        return -1;
        }
        parent->children[parent->children_cnt] = child;
        parent->children_cnt++;
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
        /* TODO subscriber the NFs that are subbed to parent to this event */
        return add_event_node_child(cur, child);
}

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

#endif  // _ONVM_EVENT_H_
