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
#include <rte_malloc.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_event.h"

#define NF_TAG "flow_sub_mos"
#include "/users/weicuidi/onvm-mos/core/src/include/mos_api.h"
#include "/users/weicuidi/onvm-mos/core/src/include/cpu.h"
#define TBL_SIZE 100
#define EXPIRE_TIME 5
#define Controller_id 2

/* number of package between each print */
static uint32_t print_delay = 1000000;

static uint32_t destination;
static uint32_t local_id;

//static struct event_retrieve_data *event_data = NULL;
/*Struct that holds all NF state information */
struct state_info {
        struct onvm_ft *ft;
        uint16_t destination;
        uint16_t print_delay;
        uint16_t num_stored;
        uint64_t elapsed_cycles;
        uint64_t last_cycles;
};

struct subscription {
        uint64_t event_id;
        void (*event_cb)(void);
};
struct subscription *subscriptions[16];

/*Struct that holds info about each flow, and is stored at each flow table entry */
struct flow_stats {
        int pkt_count;
        uint64_t last_pkt_cycles;
        int is_active;
};

struct state_info *state_info;
void nf_setup(struct onvm_nf_local_ctx *nf_local_ctx);
void msg_handler(void *msg_data, struct onvm_nf_local_ctx *nf_local_ctx);
void print_comrad(void);
void print_comrad2(void);
void print_comrad3(void);

struct rte_mempool *pubsub_msg_pool;
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
void
PrintBuff(char *buf)
{
	//struct rte_ether_hdr *ethh;
        struct ethhdr *ethh;
	struct iphdr *iph;
	//struct udphdr *udph;
	//struct tcphdr *tcph;
	uint8_t *t;
	printf("PrintPacket+++++++++++++++++++++++++++++++\n");

	ethh = (struct ethhdr *)buf;
	if (ntohs(ethh->h_proto) != ETH_P_IP) {
		printf("PrintPacket ETH_P_IP+++++++++++++\n");
	}
        #if 1
	iph = (struct iphdr *)(ethh + 1);
	//udph = (struct udphdr *)((uint32_t *)iph + iph->ihl);
	//tcph = (struct tcphdr *)((uint32_t *)iph + iph->ihl);

	t = (uint8_t *)&iph->saddr;
	char ipsrc[128];
	sprintf(ipsrc, "%u.%u.%u.%u", t[0], t[1], t[2], t[3]);
	printf("IP src:%s\n",ipsrc);
	if (iph->protocol == IPPROTO_TCP || iph->protocol == IPPROTO_UDP){
		printf("TCP or UDP\n");
	}

	t = (uint8_t *)&iph->daddr;
	char ipdst[128];
	sprintf(ipdst, "%u.%u.%u.%u", t[0], t[1], t[2], t[3]);
	printf("IP dst:%s\n",ipdst);
        #endif
	printf("PrintPacket+++++++++++++++++++end\n");
}
#endif

#if 0
void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx) {
	printf("Hi boi %d\n", nf_local_ctx->nf->instance_id);

	struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
        msg->type = RETRIEVE;
        struct event_retrieve_data *data = rte_zmalloc("ev ret data", sizeof(struct event_retrieve_data), 0);
        msg->msg_data = (void *)data;
        onvm_nflib_send_msg_to_nf(2, (void*)msg);
	
	while (data->done != 1)
                sleep(1);

	subscribe_nf_noflow(get_event(data->root, FLOW_TCP_SYN_EVENT_ID), 3);
	subscribe_nf_noflow(get_event(data->root, FLOW_TCP_ESTABLISH_EVENT_ID), 3);
	subscribe_nf_noflow(get_event(data->root, FLOW_TCP_END_EVENT_ID), 3);
        printf("Trying to get pubsub_msg_pool...\n");
        get_event_mempool(Controller_id,&local_id);
}
#else
void
nf_setup(struct onvm_nf_local_ctx *nf_local_ctx) {
	printf("Hi boi %d\n", nf_local_ctx->nf->instance_id);
        struct event_msg *msg;
        int ret = rte_mempool_get(pubsub_msg_pool, (void**)&msg);
        if (ret != 0) {
            RTE_LOG(INFO, APP, "Unable to allocate pubsub_msg_pool from pool when trying to send msg to nf\n");
            return;
        }
	
	msg->type = RETRIEVE;
        struct event_retrieve_data *data = rte_zmalloc("ev ret data", sizeof(struct event_retrieve_data), 0);
	msg->retrieve = data;
        onvm_nflib_send_msg_to_nf(Controller_id, (void*)msg);
	
	while (data->done != 1)
                sleep(1);

	subscribe_nf_noflow(get_event(data->root, FLOW_TCP_SYN_EVENT_ID), 3);
	subscribe_nf_noflow(get_event(data->root, FLOW_TCP_ESTABLISH_EVENT_ID), 3);
	subscribe_nf_noflow(get_event(data->root, FLOW_TCP_END_EVENT_ID), 3);
        printf("nf_setup Done...\n");
}
#endif

void 
event_inform(struct event_send_msg *msg){
        if(msg->event_id==FLOW_TCP_SYN_EVENT_ID)
	{
		printf("************** FLOW_TCP_SYN_EVENT_ID  pktCount***********\n");
                struct tcp_syn_event *syn_pkt = msg->pkt;
                if(syn_pkt != NULL){
                        printf("syn_pkt->flow_id:%d\n",syn_pkt->flow_id);
                        print_pkt(syn_pkt->mbuf);
                }

	}
	else if(msg->event_id==FLOW_TCP_ESTABLISH_EVENT_ID){
	        printf("************** FLOW_TCP_ESTABLISH_EVENT_ID pktCount***********\n");
                struct tcp_established_event *tcp_est = msg->pkt;
                if(tcp_est != NULL){
                        print_pkt(tcp_est->mbuf);
                }
                printf("pkt_direction:%d\n",tcp_est->pkt_direction);
	}
	else if(msg->event_id==FLOW_TCP_END_EVENT_ID){
		printf("************** FLOW_TCP_END_EVENT_ID  pktCount***********\n");
	}

        #if 0
        struct rte_mbuf* data1 = (struct rte_mbuf*)msg->pkt;
        if(data1!=NULL)
        {
                print_pkt(data1);
        }
        #endif
}

//int pktCount = 0;
void
msg_handler(void *msg_data, struct onvm_nf_local_ctx *nf_local_ctx){

	//printf("NF %d recieved msg\n", nf_local_ctx->nf->instance_id);
	struct event_msg *msg1 = (struct event_msg *)msg_data;	
        
        if (msg1->type == SEND){
                struct event_send_msg *msg = (struct event_send_msg *)&(msg1->send);
                
                if(msg!=NULL)
                {
                        if(msg->pkt != NULL){
                                //event_inform(msg);
                                pubsub_msg_pool_put((void*)msg->pkt);
                        }
                        //event_inform(msg);
                        pubsub_msg_pool_put((void*)msg);
                }
                else{
                        printf("msg is NULL\n");
                }
                pubsub_msg_pool_put((void*)msg1);
        }else {
                printf("Recieved unknown event msg type - %d\n", msg1->type);
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
	if (pkt->port == 0) {
                meta->destination = 1;
        } else {
                meta->destination = 0;
        }
        meta->action = ONVM_NF_ACTION_OUT;
        return 0;
}

int
main(int argc, char *argv[]) {
        struct onvm_nf_local_ctx *nf_local_ctx;
        struct onvm_nf_function_table *nf_function_table;
        int arg_offset;

        const char *progname = argv[0];
        local_id = strtoul(argv[4], NULL, 10);

        nf_local_ctx = onvm_nflib_init_nf_local_ctx();
        onvm_nflib_start_signal_handler(nf_local_ctx, NULL);

        nf_function_table = onvm_nflib_init_nf_function_table();
        nf_function_table->pkt_handler = &packet_handler;
	nf_function_table->setup = &nf_setup;
        nf_function_table->msg_handler = &msg_handler;

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, nf_local_ctx, nf_function_table)) < 0) {
                onvm_nflib_stop(nf_local_ctx);
                if (arg_offset == ONVM_SIGNAL_TERMINATION) {
                        printf("Exiting due to user termination\n");
                        return 0;
                } else {
                        rte_exit(EXIT_FAILURE, "Failed ONVM init\n");
                }
        }

        pubsub_msg_pool = lookup_pubsub_msg_pool();
        if(pubsub_msg_pool == NULL)
        {
                printf("Cannot find pubsub_msg_pool...\n");
                exit(-1);
        }
        else{
                printf("Find pubsub_msg_pool...\n");
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
