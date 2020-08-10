/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2019 George Washington University
 *            2015-2019 University of California Riverside
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
 * forward.c - an example using onvm. Forwards packets to a DST NF.
 ********************************************************************/

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_ip.h>
#include <rte_mbuf.h>
#include <rte_memcpy.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_event.h"

#define NF_TAG "controller_mos"

/*****************EVENT TYPES************************/
struct event_tree_node *ROOT_EVENT;

struct event_tree_node *PKT_EVENT;
struct event_tree_node *PKT_TCP_EVENT;
struct event_tree_node *PKT_TCP_SYN_EVENT;
struct event_tree_node *PKT_TCP_FIN_EVENT;
struct event_tree_node *PKT_TCP_DPI_EVENT;

struct event_tree_node *FLOW_EVENT;
struct event_tree_ndoe *FLOW_TCP_EVENT;
struct event_tree_node *FLOW_TCP_TERM_EVENT;

struct event_tree_node *STATS_EVENT;

struct event_tree_node *FLOW_REQ_EVENT;

struct event_tree_node *FLOW_DEST_EVENT;

struct event_tree_node *DATA_RDY_EVENT;
/****************************************************/
struct event_tree_node **events;

/* number of package between each print */
static uint32_t print_delay = 1000000;

static uint32_t destination;

void nf_msg_handler(void *msg_data, struct onvm_nf_local_ctx *nf_local_ctx);
static void send_event(uint64_t event_id, void *msg);
void nf_setup(struct onvm_nf_local_ctx *nf_local_ctx);

/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] -- -d <destination> -p <print_delay>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-d <dst>`: destination service ID to foward to\n");
        printf(" - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.\n");
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c, dst_flag = 0;

        while ((c = getopt(argc, argv, "d:p:")) != -1) {
                switch (c) {
                        case 'd':
                                destination = strtoul(optarg, NULL, 10);
                                dst_flag = 1;
                                break;
                        case 'p':
                                print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'd')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                                else if (optopt == 'p')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                                else if (isprint(optopt))
                                        RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                                else
                                        RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                                return -1;
                        default:
                                usage(progname);
                                return -1;
                }
        }

        if (!dst_flag) {
                RTE_LOG(INFO, APP, "Simple Forward NF requires destination flag -d.\n");
                return -1;
        }

        return optind;
}

/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(struct rte_mbuf *pkt) {
        const char clr[] = {27, '[', '2', 'J', '\0'};
        const char topLeft[] = {27, '[', '1', ';', '1', 'H', '\0'};
        static uint64_t pkt_process = 0;
        struct ipv4_hdr *ip;

        pkt_process += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PACKETS\n");
        printf("-----\n");
        printf("Port : %d\n", pkt->port);
        printf("Size : %d\n", pkt->pkt_len);
        printf("N°   : %" PRIu64 "\n", pkt_process);
        printf("\n\n");

        ip = onvm_pkt_ipv4_hdr(pkt);
        if (ip != NULL) {
                onvm_pkt_print(pkt);
        } else {
                printf("No IP4 header found\n");
        }
}

#if 0
static void
send_event(uint64_t event_id, void *pkt)
{
        //printf("send_event:event_id:%ld\n",event_id);
	int i;
	uint16_t nf_id;
	struct event_tree_node *event;
       
	struct event_tree_node *root = events[ROOT_EVENT_ID];
	event = get_event(root, event_id);
        //printf("event->subscriber_cnt:%d\n",event->subscriber_cnt);
        
	for (i = 0; i < event->subscriber_cnt; i++) {
                //printf("send_event++++++++++++1 i:%d\n",i);
                //printf("event->subscribers[i]->id:%d\n",event->subscribers[i]->id);
		nf_id = event->subscribers[i]->id;
		send_event_data(event_id,nf_id,pkt);
	}
        
}
#endif
static void
send_event(uint64_t event_id, void *msg)
{
        //printf("send_event:event_id:%ld\n",event_id);
	int i;
	uint16_t nf_id;
	struct event_tree_node *event;
        int ret;
       
	struct event_tree_node *root = events[ROOT_EVENT_ID];
	event = get_event(root, event_id);
        //printf("event->subscriber_cnt:%d\n",event->subscriber_cnt);
        
	for (i = 0; i < event->subscriber_cnt; i++) {
                //printf("event->subscribers[i]->id:%d\n",event->subscribers[i]->id);
		nf_id = event->subscribers[i]->id;
		ret = onvm_nflib_send_msg_to_nf(nf_id, msg);
                while (ret != 0)
                {
                       ret = onvm_nflib_send_msg_to_nf(nf_id, msg);
                }
	}
        
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta,
               __attribute__((unused)) struct onvm_nf_local_ctx *nf_local_ctx) {
        static uint32_t counter = 0;
        if (++counter == print_delay) {
                do_stats_display(pkt);
                counter = 0;
        }

        meta->action = ONVM_NF_ACTION_TONF;
        meta->destination = destination;
        return 0;
}
void
nf_msg_handler(void *msg_data, struct onvm_nf_local_ctx *nf_local_ctx) {
        //event_msg -> pub_sub_msg
        struct event_msg *event_msg = (struct event_msg*)msg_data;

        nf_local_ctx->nf = nf_local_ctx->nf;
        if (event_msg->type == SUBSCRIBE) {
                struct event_subscribe_data *msg = (struct event_subscribe_data *)event_msg->data;
                subscribe_nf(msg->event, msg->id, msg->flow_id);
        } else if (event_msg->type == RETRIEVE) {
                struct event_retrieve_data *data = (struct event_retrieve_data*)event_msg->data;
                data->root = events[ROOT_EVENT_ID];
                data->done = 1;
        } else if (event_msg->type == PUBLISH) {
                struct event_publish_data *data = (struct event_publish_data*)event_msg->data;
                add_event(ROOT_EVENT, data->event);
                events[data->event->event_id] = data->event;
                data->done = 1;
        } else if (event_msg->type == SEND){
                //change onvm_event_msg to event_send_msg;
		struct onvm_event_msg *event_msg_data = (struct onvm_event_msg*)event_msg->data;
                /*rte_free((void*)event_msg_data->pkt);
                rte_free((void*)event_msg_data);
                rte_free((void*)event_msg);*/
                //char *data1 = (char*)(event_msg_data->pkt);
                //printf("event_msg->event_id:%ld, data1:%s\n",event_msg_data->event_id,data1);
		send_event(event_msg_data->event_id, (void*)event_msg);
	}
	else {
                printf("Recieved unknown event msg type - %d\n", event_msg->type);
        }
}

void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx) {
        events = (struct event_tree_node **) rte_calloc("root event", MAX_EVENTS, sizeof(struct event_tree_node *), 0);

        ROOT_EVENT = gen_event_tree_node(ROOT_EVENT_ID);
        PKT_EVENT = gen_event_tree_node(PKT_EVENT_ID);
        FLOW_EVENT = gen_event_tree_node(FLOW_EVENT_ID);

	struct event_tree_node *FLOW_TCP_EVENT = gen_event_tree_node(FLOW_TCP_EVENT_ID);

	struct event_tree_node* FLOW_TCP_SYN_EVENT = gen_event_tree_node(FLOW_TCP_SYN_EVENT_ID);
	struct event_tree_node* FLOW_TCP_ESTABLISH_EVENT = gen_event_tree_node(FLOW_TCP_ESTABLISH_EVENT_ID);
	struct event_tree_node* FLOW_TCP_END_EVENT = gen_event_tree_node(FLOW_TCP_END_EVENT_ID);

        add_event_node_child(ROOT_EVENT, PKT_EVENT);
        add_event_node_child(ROOT_EVENT, FLOW_EVENT);
	add_event_node_child(FLOW_EVENT, FLOW_TCP_EVENT);
	add_event_node_child(FLOW_TCP_EVENT,FLOW_TCP_SYN_EVENT);
	add_event_node_child(FLOW_TCP_EVENT,FLOW_TCP_ESTABLISH_EVENT);
	add_event_node_child(FLOW_TCP_EVENT,FLOW_TCP_END_EVENT);

        events[ROOT_EVENT_ID] = ROOT_EVENT;
        events[PKT_EVENT_ID] = PKT_EVENT;
        events[FLOW_EVENT_ID] =  FLOW_EVENT;
	events[FLOW_TCP_SYN_EVENT_ID] = FLOW_TCP_SYN_EVENT;
	events[FLOW_TCP_ESTABLISH_EVENT_ID] = FLOW_TCP_ESTABLISH_EVENT;
	events[FLOW_TCP_END_EVENT_ID] = FLOW_TCP_END_EVENT;

        nf_local_ctx->nf->data = (void *)ROOT_EVENT;
}

int
main(int argc, char *argv[]) {
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        int arg_offset;

        const char *progname = argv[0];

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;
        nf_function_table->setup = &nf_setup;
        nf_function_table->msg_handler = &nf_msg_handler;

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }

        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }

        onvm_nflib_run(nf_local_ctx);

        onvm_nflib_stop(nf_local_ctx);
        printf("If we reach here, program is ending\n");
        return 0;
}
