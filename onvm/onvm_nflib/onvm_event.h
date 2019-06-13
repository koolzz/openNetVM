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

#define RETRIEVE 1
#define SUBSCRIBE 2

/*****************EVENT IDS************************/
#define ROOT_EVENT_ID 1

#define PKT_EVENT_ID 2
#define PKT_TCP_EVENT_ID 3
#define PKT_TCP_SYN_EVENT_ID 4
#define PKT_TCP_FIN_EVENT_ID 5
#define PKT_TCP_DPI_EVENT_ID 6

#define FLOW_EVENT_ID 7
#define FLOW_TCP_EVENT_ID 8
#define FLOW_TCP_TERM_EVENT_ID 9

#define STATS_EVENT_ID 10

#define FLOW_REQ_EVENT_ID 11
#define FLOW_DEST_EVENT_ID 12

#define DATA_RDY_EVENT_ID 13
/****************************************************/

struct event_retrieve_data {
        //int event_id;
        struct event_tree_node **events;
        int done;
};

struct event_subscribe_data {
        int op; // subscribe or something else
        struct event_tree_node *event;
        uint16_t id;
        uint16_t flow_id;
};

struct event_msg {
        int type; //retrieve or subscribe
        void *data;
};

struct nf_subscriber {
        uint16_t id;
        int flows[MAX_FLOWS];
};

struct event_tree_node {
        int event_id;
        uint16_t children_cnt;
        uint16_t subscriber_cnt;
        struct event_tree_node *parent;
        struct event_tree_node *children[MAX_EVENTS]; // sub events
        struct nf_subscriber *subscribers[MAX_SUBSCRIBERS];
};

struct event_tree {
        struct onvm_tree_node *children[MAX_EVENTS]; // sub events
};

struct event_tree_node *gen_event_tree_node(int event_id);
struct nf_subscriber *gen_nf_subscriber(void);
void subscribe_nf(struct event_tree_node *event, uint16_t id, uint16_t flow_id);
void print_targets(struct event_tree_node *event);
void add_event_node_child(struct event_tree_node *parent, struct event_tree_node *child);
int subscribed_to_event(struct event_tree_node *event, uint16_t nf_id, uint16_t flow_id);

struct event_tree_node *
gen_event_tree_node(int event_id) {
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

void
add_event_node_child(struct event_tree_node *parent, struct event_tree_node *child) {
        parent->children[parent->children_cnt] = child;
        parent->children_cnt++;
}

void
subscribe_nf(struct event_tree_node *event, uint16_t id, uint16_t flow_id) {
        int i;
        
        event->subscribers[event->subscriber_cnt] = gen_nf_subscriber();
        event->subscribers[event->subscriber_cnt]->id = id;
        event->subscribers[event->subscriber_cnt]->flows[flow_id] = flow_id;

        for (i = 0; i < MAX_EVENTS; i++) {
                subscribe_nf(event->children[i], id, flow_id);
        }
}

int
subscribed_to_event(struct event_tree_node *event, uint16_t nf_id, uint16_t flow_id) {
        int i;
        int subscribed = 0;

        for (i = 0; i < event->subscriber_cnt; i++) {
                if (event->subscribers[i]->id == nf_id) {
                        // TODO flow id stuff
                        flow_id++;
                        subscribed = 1;
                        break;
                }
        }
        return subscribed;
}

void
print_targets(struct event_tree_node *event) {
        int i;
        printf("Send event %d to : ", event->event_id);
        for (i = 0; i < MAX_EVENTS; i++) {
                if (event->subscribers[i] != NULL)
                        printf("%d, ", event->subscribers[i]->id);
        }
        printf("\n");
}

#endif  // _ONVM_EVENT_H_
