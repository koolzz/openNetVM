/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
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
 * flow_tracker.c - an example using onvm. Stores incoming flows and prints info about them.
 ********************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <time.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_hash.h>
#include <rte_malloc.h>
#include <rte_cycles.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_flow_table.h"

#include "tcp_server.h"

#define NF_TAG "flow_tracker"
#define TBL_SIZE 100
#define EXPIRE_TIME 5

/*Struct that holds all NF state information */
struct state_info {
        struct onvm_ft *ft;
        uint16_t destination;
        uint16_t print_delay;
        uint16_t num_stored;
        uint64_t elapsed_cycles;
        uint64_t last_cycles;
};

/*Struct that holds info about each flow, and is stored at each flow table entry */
struct flow_info {
        uint8_t s_addr_bytes[ETHER_ADDR_LEN];
        int pkt_count;
        uint64_t last_pkt_cycles;
        int is_active;
        uint32_t starting_seq;
        enum tcp_state state;
};

/*Struct that holds nf meta info */
struct onvm_nf_info *nf_info;
struct state_info *state_info;

struct rte_mempool *pktmbuf_pool;

/*
 * Prints application arguments
 */
static void
usage(const char *progname) {
        printf("Usage:\n");
        printf("%s [EAL args] -- [NF_LIB args] --d <destination>\n", progname);
        printf("%s -F <CONFIG_FILE.json> [EAL args] -- [NF_LIB args] -- [NF args]\n\n", progname);
        printf("Flags:\n");
        printf(" - `-d <destination_id>`: Service ID to send packets to`\n");
        printf(" - `-p <print_delay>`:  Number of seconds between each print (default is 5)\n");
}

/*
 * Loops through inputted arguments and assigns values as necessary
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;//, dst_flag = 0;

        while ((c = getopt(argc, argv, "d:p:")) != -1) {
                switch(c) {
                        case 'd':
                                state_info->destination = strtoul(optarg, NULL, 10);
                                RTE_LOG(INFO, APP, "Sending packets to service ID %d\n", state_info->destination);
                                break;
                        case 'p':
                                state_info->print_delay = strtoul(optarg, NULL, 10);
                                break;
                        case '?':
                                usage(progname);
                                if (optopt == 'd')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument\n", optopt);
                                else if (optopt == 'p')
                                        RTE_LOG(INFO, APP, "Option -%c requires an argument\n", optopt);
                                else
                                        RTE_LOG(INFO, APP, "Unknown option character\n");
                                return -1;
                        default:
                                usage(progname);
                                return -1;
                }
        }

        return optind;
}

/*
 * Updates flow status to be "active" or "expired"
 */
static int
update_status(uint64_t elapsed_cycles, struct flow_info *data) {
        if (unlikely(data == NULL)) {
                return -1;
        }
        if ((elapsed_cycles - data->last_pkt_cycles) / rte_get_timer_hz() >= EXPIRE_TIME) {
                data->is_active = 0;
        } else {
                data->is_active = 1;
        }

        return 0;
}

/*
 * Clears expired entries from the flow table
 */
static int
clear_entries(struct state_info *state_info) {
        if (unlikely(state_info == NULL)) {
                return -1;
        }

        printf("Clearing expired entries\n");
        struct flow_info *data = NULL;
        struct onvm_ft_ipv4_5tuple *key = NULL;
        uint32_t next = 0;
        int ret = 0;

        while (onvm_ft_iterate(state_info->ft, (const void **)&key, (void **)&data, &next) > -1) {
                if (update_status(state_info->elapsed_cycles, data) < 0) {
                        return -1;
                }

                if (!data->is_active) {
                        ret = onvm_ft_remove_key(state_info->ft, key);
                        state_info->num_stored--;
                        if (ret < 0) {
                                printf("Key should have been removed, but was not\n");
                                state_info->num_stored++;
                        }
                }
        }

        return 0;
}

/*
 * Prints out information about flows stored in table
 */
static void
do_stats_display(void) {
        struct flow_info *data = NULL;
        struct onvm_ft_ipv4_5tuple *key = NULL;
        uint32_t next = 0;
        int32_t index;

        printf("------------------------------\n");
        printf("     Flow Table Contents\n");
        printf("------------------------------\n");
        printf("Current capacity: %d / %d\n\n", state_info->num_stored, TBL_SIZE);
        while ((index = onvm_ft_iterate(state_info->ft, (const void **)&key, (void **)&data, &next)) > -1) {
                update_status(state_info->elapsed_cycles, data);
                printf("%d. Status: ", index);
                if (data->is_active) {
                        printf("Active\n");
                }
                else {
                        printf("Expired\n");
                }

                printf("Key information:\n");
                _onvm_ft_print_key(key);
                printf("Packet count: %d\n\n", data->pkt_count);
        }
}

/*
 * Adds an entry to the flow table. It first checks if the table is full, and
 * if so, it calls clear_entries() to free up space.
 */
static int
table_add_entry(struct onvm_ft_ipv4_5tuple* key, struct flow_info **flow) {
        struct flow_info *data = NULL;

        if (unlikely(key == NULL || state_info == NULL || flow == NULL)) {
                return -1;
        }

        if (TBL_SIZE - state_info->num_stored == 0) {
                int ret = clear_entries(state_info);
                if (ret < 0) {
                        return -1;
                }
        }

        int tbl_index = onvm_ft_add_key(state_info->ft, key, (char **)&data);
        if (tbl_index < 0) {
                return -1;
        }

        data->pkt_count = 0;
        data->last_pkt_cycles = state_info->elapsed_cycles;
        data->is_active = 0;
        data->state = 0;
        data->starting_seq = rand();
        *flow = data;
        state_info->num_stored += 1;
        return 0;
}

/*
 * Looks up a packet hash to see if there is a matching key in the table.
 * If it finds one, it updates the metadata associated with the key entry,
 * and if it doesn't, it calls table_add_entry() to add it to the table.
 */
static int
table_lookup_entry(struct rte_mbuf* pkt, struct flow_info **flow) {
        struct flow_info *data = NULL;
        struct onvm_ft_ipv4_5tuple key;

        if (unlikely(pkt == NULL || state_info == NULL || flow == NULL)) {
                return -1;
        }

        int ret = onvm_ft_fill_key_symmetric(&key, pkt);
        if (ret < 0)
                return -1;

        int tbl_index = onvm_ft_lookup_key(state_info->ft, &key, (char **)&data);
        if (tbl_index == -ENOENT) {
                return table_add_entry(&key, flow);
        } else if (tbl_index < 0) {
                printf("Some other error occurred with the packet hashing\n");
                return -1;
        } else {
                data->pkt_count += 1;
                data->last_pkt_cycles = state_info->elapsed_cycles;
                *flow = data;
                return 0;
        }
}

static int
callback_handler(__attribute__((unused)) struct onvm_nf_info *nf_info) {
        state_info->elapsed_cycles = rte_get_tsc_cycles();

        if ((state_info->elapsed_cycles - state_info->last_cycles) / rte_get_timer_hz() > state_info->print_delay) {
                state_info->last_cycles = state_info->elapsed_cycles;
                do_stats_display();
        }

        return 0;
}

static int
handle_pkt(struct rte_mbuf *pkt, struct flow_info *flow, struct onvm_nf_info *nf_info) {
        struct rte_mbuf *response_pkt;
        struct ether_hdr *ether_hdr;
        struct ipv4_hdr *ip_hdr;
        struct tcp_hdr *tcp_hdr;
        uint8_t *payload;
        int payload_length;
        uint8_t *tcp_options;
        int tcp_options_length;

        response_pkt = NULL;
        ether_hdr = onvm_pkt_ether_hdr(pkt);
        ip_hdr = onvm_pkt_ipv4_hdr(pkt);
        tcp_hdr = onvm_pkt_tcp_hdr(pkt);

        int data_off = rte_cpu_to_be_16(tcp_hdr->data_off);
        // 8 because rte_cpu_to_be_16 but data_off is uint8_t, 4 because actual data_off in the header is 4 bits
        data_off = data_off >> 12;
        printf("data_off  %d\n", data_off);
        if (data_off > 5) {
                tcp_options = (uint8_t *)tcp_hdr + sizeof(struct tcp_hdr);
                tcp_options_length = (data_off - 5) * 4;
        } else {
                tcp_options = NULL;
                tcp_options_length = 0;
        }

        payload = (uint8_t *)tcp_hdr + sizeof(struct tcp_hdr) + tcp_options_length;
        payload_length = rte_cpu_to_be_16(ip_hdr->total_length) - sizeof(struct ipv4_hdr) - sizeof(struct tcp_hdr) - tcp_options_length;

        printf("debugging headler len -> %d\n", tcp_hdr->data_off);

        if (tcp_hdr->tcp_flags & TCP_FLAG_SYN && tcp_hdr->tcp_flags & TCP_FLAG_ACK) {
                printf("Recieved SYN ACK !!!\n");
                printf("Active open isn't available\n");
        } else if (tcp_hdr->tcp_flags & TCP_FLAG_SYN) {
                printf("Recieved SYN !!!\n");

                flow->state = TCP_ST_SYN_RCVD;

                tcp_hdr->tcp_flags |= TCP_FLAG_ACK;
                tcp_hdr->recv_ack =  tcp_hdr->sent_seq + rte_cpu_to_be_32(1);
                tcp_hdr->sent_seq = rte_cpu_to_be_32(flow->starting_seq);

                onvm_pkt_swap_ether_hdr(ether_hdr);
                onvm_pkt_swap_tcp_hdr(tcp_hdr);
                onvm_pkt_swap_ip_hdr(ip_hdr);

                response_pkt = onvm_pkt_generate_tcp(pktmbuf_pool, tcp_hdr, ip_hdr, ether_hdr, tcp_options, tcp_options_length, NULL, 0);
        } else if (tcp_hdr->tcp_flags & TCP_FLAG_ACK && tcp_hdr->tcp_flags & TCP_FLAG_PSH) {
                printf("Recieved ACK & PSH !!!\n");
                printf("The %d\n", rte_cpu_to_be_16(ip_hdr->total_length));

                if (flow->state != TCP_ST_ESTABLISHED)
                        printf("BAD Terrible THINGS\n");

                uint32_t temp_seq = tcp_hdr->sent_seq;
                tcp_hdr->sent_seq = tcp_hdr->recv_ack;
                tcp_hdr->recv_ack = temp_seq + rte_cpu_to_be_32(payload_length);

                onvm_pkt_swap_ether_hdr(ether_hdr);
                onvm_pkt_swap_tcp_hdr(tcp_hdr);
                onvm_pkt_swap_ip_hdr(ip_hdr);

                response_pkt = onvm_pkt_generate_tcp(pktmbuf_pool, tcp_hdr, ip_hdr, ether_hdr, tcp_options, tcp_options_length, payload, payload_length);
        } else if (tcp_hdr->tcp_flags & TCP_FLAG_ACK && tcp_hdr->tcp_flags & TCP_FLAG_FIN) {
                printf("Recieved FIN ACK !!!\n");

                if (flow->state != TCP_ST_ESTABLISHED && flow->state != TCP_ST_CLOSE_WAIT)
                        printf("BAD BAD THINGS\n");

                if (flow->state == TCP_ST_ESTABLISHED)
                        flow->state = TCP_ST_CLOSE_WAIT;
                else if (flow->state == TCP_ST_CLOSE_WAIT)
                        flow->state = TCP_ST_LAST_ACK;

                tcp_hdr->tcp_flags = 0;
                tcp_hdr->tcp_flags |= TCP_FLAG_ACK;

                uint32_t temp_seq = tcp_hdr->sent_seq;
                tcp_hdr->sent_seq =  tcp_hdr->recv_ack;
                tcp_hdr->recv_ack =  temp_seq + rte_cpu_to_be_32(1);

                onvm_pkt_swap_ether_hdr(ether_hdr);
                onvm_pkt_swap_tcp_hdr(tcp_hdr);
                onvm_pkt_swap_ip_hdr(ip_hdr);

                response_pkt = onvm_pkt_generate_tcp(pktmbuf_pool, tcp_hdr, ip_hdr, ether_hdr, tcp_options, tcp_options_length, NULL, 0);
        } else if (tcp_hdr->tcp_flags & TCP_FLAG_ACK) {
                printf("Recieved ACK !!!\n");
                if (flow->state == TCP_ST_SYN_RCVD) {
                        flow->state = TCP_ST_ESTABLISHED;
                } else {
                        printf("What is this packet\n");
                }

        }


        if (response_pkt == NULL) {
                return 0;
        }

        ether_hdr = onvm_pkt_ether_hdr(response_pkt);
        ip_hdr = onvm_pkt_ipv4_hdr(response_pkt);
        tcp_hdr = onvm_pkt_tcp_hdr(response_pkt);

        struct onvm_pkt_meta *pmeta = onvm_get_pkt_meta(response_pkt);
        response_pkt->port = pkt->port;
        pmeta->destination = response_pkt->port;
        pmeta->action = ONVM_NF_ACTION_OUT;
        onvm_pkt_set_checksums(response_pkt);
        onvm_nflib_return_pkt(nf_info, response_pkt);

        return 0;
}

static int
packet_handler(struct rte_mbuf *pkt, struct onvm_pkt_meta *meta, __attribute__((unused)) struct onvm_nf_info *nf_info) {
        struct ether_hdr *ehdr;
        struct flow_info *flow = rte_malloc("flow info", sizeof(struct flow_info), 0);
        int i;

        if (!onvm_pkt_is_ipv4(pkt) || !onvm_pkt_is_tcp(pkt)) {
                meta->destination = 0;//state_info->destination;
                meta->action = ONVM_NF_ACTION_DROP;
                return 0;
        }

        if (table_lookup_entry(pkt, &flow) < 0) {
                printf("Packet could not be identified or processed\n");
                meta->destination = 0;//state_info->destination;
                meta->action = ONVM_NF_ACTION_DROP;
                return 0;
        }

        if (flow->is_active == 0) {
                flow->is_active = 1;
                ehdr = onvm_pkt_ether_hdr(pkt);
                for (i = 0; i < ETHER_ADDR_LEN; i++) { 
                        flow->s_addr_bytes[i] = ehdr->s_addr.addr_bytes[i];
                }
        }

        //onvm_pkt_print(pkt);

        handle_pkt(pkt, flow, nf_info);

        meta->destination = 0;//state_info->destination;
        meta->action = ONVM_NF_ACTION_DROP;

        return 0;
}

int
main(int argc, char *argv[]) {
        int arg_offset;
        const char *progname = argv[0];

        srand(time(NULL));

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG, &nf_info)) < 0)
                return -1;

        argc -= arg_offset;
        argv += arg_offset;

        state_info = rte_calloc("state", 1, sizeof(struct state_info), 0);
        if (state_info == NULL) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Unable to initialize NF state");
        }

        state_info->print_delay = 5;
        state_info->num_stored = 0;

        if (parse_app_args(argc, argv, progname) < 0) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments");
        }

        pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);

        state_info->ft = onvm_ft_create(TBL_SIZE, sizeof(struct flow_info));
        if (state_info->ft == NULL) {
                onvm_nflib_stop(nf_info);
                rte_exit(EXIT_FAILURE, "Unable to create flow table");
        }

        /*Initialize NF timer */
        state_info->elapsed_cycles = rte_get_tsc_cycles();

        onvm_nflib_run_callback(nf_info, &packet_handler, &callback_handler);

        printf("If we reach here, program is ending!\n");
        return 0;
}
