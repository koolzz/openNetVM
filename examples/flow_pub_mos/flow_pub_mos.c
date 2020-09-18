#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <asm/byteorder.h>
#include <assert.h>
#include <signal.h>
#include <sys/queue.h>
#include <errno.h>

#include <mos_api.h>
#include "cpu.h"

#include "onvm_flow_table.h"
#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_event.h"
#define ONVM

/* Maximum CPU cores */
#define MAX_CORES 		16
/* Number of TCP flags to monitor */
#define NUM_FLAG 		6
/* Default path to mOS configuration file */
#define MOS_CONFIG_FILE		"config/mos-onvm.conf"
//#define MOS_CONFIG_FILE               "config/mos.conf"

#define SENDY 1
#define SEND_ENABLE 1
#define PUB_POOL 1

//#define TIME_STAT
#ifdef TIME_STAT
#define PRINT_STAT 0
#include "app_stat.h"
struct stat_counter stat_cb_st_chg, stat_cb_creation, stat_cb_destroy;
struct stat_counter stat_cb_cnt, stat_cb_content, stat_cb_flow_content;
struct stat_counter stat_cb_st_new, stat_cb_st_end, stat_cb_st_establish;
#endif
uint64_t alert_cnt = 0;
struct rte_mempool *pubsub_msg_pool;

//int STATE_FLAG = -1;
/*----------------------------------------------------------------------------*/
/* Global variables */

struct connection {
	int sock;                      /* socket ID */
	struct sockaddr_in addrs[2];   /* Address of a client and a serer */
	int cli_state;                 /* TCP state of the client */
	int svr_state;                 /* TCP state of the server */
	TAILQ_ENTRY(connection) link;  /* link to next context in this core */
};

int g_max_cores;                              /* Number of CPU cores to be used */
mctx_t g_mctx[MAX_CORES];                     /* mOS context */
TAILQ_HEAD(, connection) g_sockq[MAX_CORES];  /* connection queue */
uint64_t g_cli_cnt = 0, g_svr_cnt = 0;
#ifdef ONVM
int g_run_core;
int destination_id;
#endif

//int testcount = 0;
/*----------------------------------------------------------------------------*/
#if 0
void
PrintBuff(char *buf)
{
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
	printf("PrintPacket+++++++++++++++++++end\n");
}
#endif
/*----------------------------------------------------------------------------*/
/* Signal handler */
static void
sigint_handler(int signum)
{
	int i;

	/* Terminate the program if any interrupt happens */
	for (i = 0; i < g_max_cores; i++)
		mtcp_destroy_context(g_mctx[i]);
}

/*----------------------------------------------------------------------------*/
/*Init event to try to connect with controller*/
#if PUB_POOL
static int event_init(uint16_t dest_controller)
{
	printf("event_init+++++++++++++++++\n");
	struct event_msg *msg;
    int ret = rte_mempool_get(pubsub_msg_pool, (void**)&msg);
    if (ret != 0) {
        RTE_LOG(INFO, APP, "Unable to allocate pubsub_msg_pool from pool when trying to send msg to nf\n");
        return ret;
    }
	
	msg->type = RETRIEVE;
	//struct event_retrieve_data *data;
	//ret = rte_mempool_get(pubsub_msg_pool, (void**)&data);
	struct event_retrieve_data *data = rte_zmalloc("ev ret data", sizeof(struct event_retrieve_data), 0);
	msg->retrieve = data;
	//struct event_retrieve_data* data = msg->retrieve;
	onvm_nflib_send_msg_to_nf(dest_controller, (void*)msg);
	//struct event_retrieve_data* data = msg->retrieve;
	while (data->done != 1)
		sleep(1);
	publish_event(dest_controller,FLOW_TCP_SYN_EVENT_ID);
	publish_event(dest_controller,FLOW_TCP_ESTABLISH_EVENT_ID);
	publish_event(dest_controller,FLOW_TCP_END_EVENT_ID);

	printf("event_init done++++dest_controller:%d+++++++++\n",dest_controller);
	return 0;
}
#else
static void event_init(uint16_t dest_controller)
{
	printf("event_init+++++++++++++++++\n");
	struct event_msg *msg = rte_zmalloc("ev msg", sizeof(struct event_msg), 0);
	msg->type = RETRIEVE;
	struct event_retrieve_data *data = rte_zmalloc("ev ret data", sizeof(struct event_retrieve_data), 0);
	msg->data = (void *)data;
	onvm_nflib_send_msg_to_nf(dest_controller, (void*)msg);
	while (data->done != 1)
		sleep(1);

	publish_event(dest_controller,FLOW_TCP_SYN_EVENT_ID);
	publish_event(dest_controller,FLOW_TCP_ESTABLISH_EVENT_ID);
	publish_event(dest_controller,FLOW_TCP_END_EVENT_ID);

	printf("event_init done++++dest_controller:%d+++++++++\n",dest_controller);
}
#endif
/*----------------------------------------------------------------------------*/
/* Find connection structure by socket ID */
static inline struct connection *
find_connection(int cpu, int sock)
{
	struct connection *c;

	TAILQ_FOREACH(c, &g_sockq[cpu], link)
		if (c->sock == sock)
			return c;

	return NULL;
}
/*----------------------------------------------------------------------------*/
/* Create connection structure for new connection */
#if 0
static void
cb_creation(mctx_t mctx, int sock, int side, uint64_t events, filter_arg_t *arg)
{
	printf("+++++++++++++++++++cb_creation++++++++++++++\n");
	STATE_FLAG = TCP_LISTEN;
	#if 1
	#ifdef TIME_STAT
	unsigned long long start_tsc = rdtscll();
	#endif	
	socklen_t addrslen = sizeof(struct sockaddr) * 2;
	struct connection *c;

	c = calloc(sizeof(struct connection), 1);
	if (!c)
		return;

	/* Fill values of the connection structure */
	c->sock = sock;
	if (mtcp_getpeername(mctx, c->sock, (void *)c->addrs, &addrslen,
						 MOS_SIDE_CLI) < 0) {
		perror("mtcp_getpeername");
		/* it's better to stop here and do debugging */
		exit(EXIT_FAILURE); 
	}
	#endif
	 
	//char *msg_sent = NULL;
	//printf("Send SYN notification testcount:%d\n",++testcount);	
	//send_event_data(FLOW_TCP_SYN_EVENT_ID, destination_id, NULL);
 
	/* Insert the structure to the queue */
	TAILQ_INSERT_TAIL(&g_sockq[mctx->cpu], c, link);
	#ifdef TIME_STAT
	UpdateStatCounter(&stat_cb_creation, rdtscll() - start_tsc);
	#endif	
}
#else
static void
cb_creation(mctx_t mctx, int sock, int side, uint64_t events, filter_arg_t *arg)
{
	//printf("+++++++++++++++++++cb_creation++++++++++++++\n");
	#if 1	
	socklen_t addrslen = sizeof(struct sockaddr) * 2;
	struct connection *c;
	c = calloc(sizeof(struct connection), 1);
	if (!c)
		return;

	/* Fill values of the connection structure */
	c->sock = sock;
	if (mtcp_getpeername(mctx, c->sock, (void *)c->addrs, &addrslen,
						//MOS_SIDE_BOTH) < 0) {
						MOS_SIDE_CLI) < 0) {
		perror("mtcp_getpeername");
		/* it's better to stop here and do debugging */
		exit(EXIT_FAILURE); 
	}
	#endif
 
	/* Insert the structure to the queue */
	TAILQ_INSERT_TAIL(&g_sockq[mctx->cpu], c, link);

	struct pkt_ctx *pi;
	if(mtcp_getlastbuf(mctx, sock, side, &pi) < 0){
		fprintf(stderr, "Failed to get packet context\n");
		exit(-1);
	}
	if(pi!=NULL)
	{
		//printf("pkt_len:%d\n",((pi->p).pkt_buf)->pkt_len);
		//printf("Send FLOW_TCP_SYN_EVENT_ID...\n");
		#if SEND_ENABLE
		send_event_data(FLOW_TCP_SYN_EVENT_ID, destination_id, (void*)(pi->p).pkt_buf);
		#endif
	}
}	
#endif
/*----------------------------------------------------------------------------*/
/* Destroy connection structure */
static void
cb_destroy(mctx_t mctx, int sock, int side, uint64_t events, filter_arg_t *arg)
{
	#ifdef TIME_STAT
	unsigned long long start_tsc = rdtscll();
	#endif	
	struct connection *c;

	if (!(c = find_connection(mctx->cpu, sock)))
		return;

	#if 0
	struct pkt_ctx *pi;
	if(mtcp_getlastbuf(mctx, sock, side, &pi) < 0){
		fprintf(stderr, "Failed to get packet context\n");
		exit(-1);
	}
	if(pi!=NULL)
	{
		printf("Send FLOW_TCP_END_EVENT_ID...\n");
		send_event_data(FLOW_TCP_END_EVENT_ID, destination_id, (void*)(pi->p).pkt_buf);
	}
	#endif

	TAILQ_REMOVE(&g_sockq[mctx->cpu], c, link);
	free(c);

	//printf("Send FLOW_TCP_END_EVENT_ID...\n");
	//send_event_data(FLOW_TCP_END_EVENT_ID, destination_id, NULL);

}

/*----------------------------------------------------------------------------*/
/* get pkt and Send event_id and pkt  */
#if 1
static void
send_pkt_to_dest(mctx_t mctx, int sock, int side, uint64_t event_id){
	struct pkt_ctx *pi;
	if(mtcp_getlastbuf(mctx, sock, side, &pi) < 0){
		fprintf(stderr, "Failed to get packet context\n");
		exit(-1);
	}
	if(pi!=NULL)
	{
		//printf("pkt_len:%d\n",((pi->p).pkt_buf)->pkt_len);
		//printf("Send FLOW_TCP_ESTABLISH_EVENT_ID...\n");
		#if SEND_ENABLE
		send_event_data(event_id, destination_id, (void*)(pi->p).pkt_buf);
		#endif
	}
	else{
		//printf("pkt is null\n");
		#if SEND_ENABLE
		send_event_data(event_id, destination_id, NULL);
		#endif
	}
}
#endif
/*----------------------------------------------------------------------------*/
/* Update connection's TCP state of each side */

static void
cb_st_chg(mctx_t mctx, int sock, int side, uint64_t events, filter_arg_t *arg)
{
	//printf("++++++++++++++++st_chg++++++++++++++\n");

	#ifdef TIME_STAT
	unsigned long long start_tsc = rdtscll();
	#endif	
	struct connection *c;
	socklen_t intlen = sizeof(int);

	if (!(c = find_connection(mctx->cpu, sock)))
		return;

	int tcp_state = -1;

	if (side == MOS_SIDE_CLI) {
		
		if (mtcp_getsockopt(mctx, c->sock, SOL_MONSOCKET, MOS_TCP_STATE_CLI,
						(void *)&c->cli_state, &intlen) < 0) {
			perror("mtcp_getsockopt\n");
			exit(-1); /* it's better to stop here and do debugging */
		}
		//printf("tcpstate:%d\n",c->cli_state);
		tcp_state = c->cli_state;
	} else {
		if (mtcp_getsockopt(mctx, c->sock, SOL_MONSOCKET, MOS_TCP_STATE_SVR,
						(void *)&c->svr_state, &intlen) < 0) {
			perror("mtcp_getsockopt\n");
			exit(-1); /* it's better to stop here and do debugging */
		}
		//printf("tcpstate:%d\n",c->svr_state);
		tcp_state = c->svr_state;
	}

	
	if(tcp_state == TCP_ESTABLISHED)
	{
		//printf(" Send TCP established!\n");
		send_pkt_to_dest(mctx, sock, side,FLOW_TCP_ESTABLISH_EVENT_ID);
	}
	//else if(tcp_state == TCP_FIN_WAIT_1 || tcp_state == TCP_FIN_WAIT_2 || tcp_state== TCP_CLOSING || tcp_state == TCP_CLOSE_WAIT)
	else if(tcp_state == TCP_CLOSED)
	{
		//printf(" Send TCP CLOSE\n");
		send_pkt_to_dest(mctx, sock, side, FLOW_TCP_END_EVENT_ID);
	}
	
	
}

/*----------------------------------------------------------------------------*/
/* Convert state value (integer) to string (char array) */
//Defined in core/src/include/mos_api.h from 0 to 10
const char *
strstate(int state)
{
	switch (state) {
#define CASE(s) case TCP_##s: return #s
		CASE(CLOSED);
		CASE(LISTEN);
		CASE(SYN_SENT);
		CASE(SYN_RCVD);
		CASE(ESTABLISHED);
		CASE(FIN_WAIT_1);
		CASE(FIN_WAIT_2);
		CASE(CLOSE_WAIT);
		CASE(CLOSING);
		CASE(LAST_ACK);
		CASE(TIME_WAIT);
		default:
		return "-";
	}
}
/*----------------------------------------------------------------------------*/
/* Print ongoing connection information based on connection structure */
#if PRINT_STAT
static void
cb_printstat(mctx_t mctx, int sock, int side,
				  uint64_t events, filter_arg_t *arg)
{
	int i;
	struct connection *c;
	struct timeval tv_1sec = { /* 1 second */
		.tv_sec = 1,
		.tv_usec = 0
	};

	printf("Proto CPU "
		   "Client Address        Client State "
		   "Server Address        Server State\n");
	for (i = 0; i < g_max_cores; i++)
		TAILQ_FOREACH(c, &g_sockq[i], link) {
			int space;

			printf("%-5s %-3d ", "tcp", i);
			space = printf("%s:", inet_ntoa(c->addrs[MOS_SIDE_CLI].sin_addr));
			printf("%*d %-12s ",
					space - 21,
					ntohs(c->addrs[MOS_SIDE_CLI].sin_port),
					strstate(c->cli_state));
			space = printf("%s:", inet_ntoa(c->addrs[MOS_SIDE_SVR].sin_addr));
			printf("%*d %-12s\n",
					space - 21,
					ntohs(c->addrs[MOS_SIDE_SVR].sin_port),
					strstate(c->svr_state));
		}
	printf ("Total Pkts: Client: %llu, Server: %llu\n", (unsigned long long)g_cli_cnt, (unsigned long long)g_svr_cnt );
	
	#ifdef TIME_STAT
	printf("Callback_Time: (avg (cycles), max (cycles)) "
			"cb_creation: (%4lu, %4lu), "
			"cb_destroy: (%4lu, %4lu), "
			"cb_st_chg: (%4lu, %4lu), "
			"cb_cnt: (%4lu, %4lu), "
			"cb_content: (%4lu, %4lu), "
			"cb_flow_content: (%4lu, %4lu)\n",
			GetAverageStat(&stat_cb_creation), stat_cb_creation.max,
			GetAverageStat(&stat_cb_destroy), stat_cb_destroy.max,
			GetAverageStat(&stat_cb_st_chg), stat_cb_st_chg.max,
			GetAverageStat(&stat_cb_cnt), stat_cb_cnt.max,
			GetAverageStat(&stat_cb_content), stat_cb_content.max,
			GetAverageStat(&stat_cb_flow_content), stat_cb_flow_content.max);
	InitStatCounter(&stat_cb_cnt);
	InitStatCounter(&stat_cb_content);
	InitStatCounter(&stat_cb_flow_content);
	#endif

	printf("APP_Info: alert_cnt: %4lu\n", alert_cnt);

	/* Set a timer for next printing */
	#if PRINT_STAT
	if (mtcp_settimer(mctx, sock, &tv_1sec, cb_printstat)) {
		fprintf(stderr, "Failed to register print timer\n");
		exit(-1); /* no point in proceeding if the timer is broken */
	}
	#endif

	return;
}
#endif

/*----------------------------------------------------------------------------*/
#if 1
/* Check connection's TCP pkt payload */
static void
cb_pkt_content(mctx_t mctx, int sock, int side, uint64_t events, filter_arg_t *arg)
{
	//printf("+++++++++++++++++++cb_pkt_content+++++++++++\n");
	#ifdef TIME_STAT
	unsigned long long start_tsc = rdtscll();
	#endif	
	//struct pkt_info pi;
	struct pkt_ctx *pi;
	if(mtcp_getlastbuf(mctx, sock, side, &pi) < 0){
		fprintf(stderr, "Failed to get packet context\n");
		exit(-1);
	}

	#ifdef TIME_STAT
	UpdateStatCounter(&stat_cb_content, rdtscll() - start_tsc);	
	#endif	

	struct connection *c;
	if (!(c = find_connection(mctx->cpu, sock)))
		return;

	if(c->cli_state == TCP_ESTABLISHED || c->svr_state == TCP_ESTABLISHED)
	{		
		#if 0
		//char *pkt_sent = NULL;
		if((strlen((char*)pi.payload) != 0) && (pi.payload != NULL))
		{
			/*pkt_sent = rte_zmalloc("ev msg", sizeof(char) * strlen((char*)pi.payload), 0);
			rte_memcpy(pkt_sent, (char*)pi.payload, strlen((char*)pi.payload));
			send_event_data(FLOW_TCP_ESTABLISH_EVENT_ID, destination_id, (void*)pkt_sent);*/
			//printf("send_event_data_msg++++++++++++1\n");
			send_event_data_msg(FLOW_TCP_ESTABLISH_EVENT_ID, destination_id, (void*)pi.payload);
			//printf("send_event_data_msg++++++++++++2\n");
		}
		else{
			//printf("establish pkt is null\n");
			send_event_data(FLOW_TCP_ESTABLISH_EVENT_ID, destination_id, NULL);
		}
		#endif
		//struct rte_mbuf *dpdk_buff = (struct rte_mbuf *)((pi->p).pkt_buf);
		//PrintBuff((char*)(pi->p).pkt_buf);
		//printf("strlen(pi.p->pkt_buf):%ld++++++++++++++\n",strlen((char*)((pi->p).pkt_buf)));
		//printf("%d\n",dpdk_buff->pkt_len);
		if(pi!=NULL)
		{
			//printf("Send FLOW_TCP_ESTABLISH_EVENT_ID...\n");
			#if SEND_ENABLE
			send_event_data(FLOW_TCP_ESTABLISH_EVENT_ID, destination_id, (void*)(pi->p).pkt_buf);
			#endif
		}
	}
}
#endif

/*----------------------------------------------------------------------------*/
/* Register required callbacks */
static void
RegisterCallbacks(mctx_t mctx, int sock, event_t ev_new_syn)
{
	printf("=============================RegisterCallbacks=====================\n");
	//publish_ev_to_controller(PUBSUB_CONTROLLER_ID,FLOW_TCP_SYN_EVENT_ID);
	
	#if PRINT_STAT
	struct timeval tv_1sec = { /* 1 second */
		.tv_sec = 1,
		.tv_usec = 0
	};
	#endif

	#ifdef TIME_STAT
	InitStatCounter(&stat_cb_creation);
	InitStatCounter(&stat_cb_destroy);
	InitStatCounter(&stat_cb_st_chg);
	InitStatCounter(&stat_cb_cnt);
	InitStatCounter(&stat_cb_content);
	InitStatCounter(&stat_cb_flow_content);
	#endif

	/* Register callbacks */
	if (mtcp_register_callback(mctx, sock, MOS_ON_CONN_START,
				   MOS_HK_SND, cb_creation)) {
		fprintf(stderr, "Failed to register cb_creation()\n");
		exit(-1); /* no point in proceeding if callback registration fails */
	}	
	if (mtcp_register_callback(mctx, sock, MOS_ON_CONN_END,
				   MOS_HK_SND, cb_destroy)) {
		fprintf(stderr, "Failed to register cb_destroy()\n");
		exit(-1); /* no point in proceeding if callback registration fails */
	}	
	if (mtcp_register_callback(mctx, sock, MOS_ON_TCP_STATE_CHANGE,
				   MOS_HK_SND, cb_st_chg)) {
		fprintf(stderr, "Failed to register cb_st_chg()\n");
		exit(-1); /* no point in proceeding if callback registration fails */
	}		
	if (mtcp_register_callback(mctx, sock, MOS_ON_TCP_STATE_CHANGE,
				   MOS_HK_RCV, cb_st_chg)) {
		fprintf(stderr, "Failed to register cb_st_chg()\n");
		exit(-1); /* no point in proceeding if callback registration fails */
	}
	if (mtcp_register_callback(mctx, sock, MOS_ON_PKT_IN,
				   MOS_HK_SND, cb_pkt_content)) {
		fprintf(stderr, "Failed to register cb_pkt_cnt()\n");
		exit(-1); 
	}

	/* CPU 0 is in charge of printing stats */
	#if PRINT_STAT
	if (mctx->cpu == 0 &&
		mtcp_settimer(mctx, sock, &tv_1sec, cb_printstat)) {
		fprintf(stderr, "Failed to register print timer\n");
		exit(-1); /* no point in proceeding if the titmer is broken*/
	}	
	#endif
}
/*----------------------------------------------------------------------------*/
/* Open monitoring socket and ready it for monitoring */
static void
InitMonitor(mctx_t mctx, event_t ev_new_syn)
{
	int sock;

	/* Initialize internal memory structures */
	TAILQ_INIT(&g_sockq[mctx->cpu]);

	/* create socket and set it as nonblocking */
	if ((sock = mtcp_socket(mctx, AF_INET,
						 MOS_SOCK_MONITOR_STREAM, 0)) < 0) {
		fprintf(stderr, "Failed to create monitor listening socket!\n");
		exit(-1); /* no point in proceeding if we don't have a listening socket */
	}

	/* Disable socket buffer */
	#if 1
	int optval = 0;
	if (mtcp_setsockopt(mctx, sock, SOL_MONSOCKET, MOS_CLIBUF,
							   &optval, sizeof(optval)) == -1) {
		fprintf(stderr, "Could not disable CLIBUF!\n");
	}
	if (mtcp_setsockopt(mctx, sock, SOL_MONSOCKET, MOS_SVRBUF,
							   &optval, sizeof(optval)) == -1) {
		fprintf(stderr, "Could not disable SVRBUF!\n");
	}
	#endif

	RegisterCallbacks(mctx, sock, ev_new_syn);
	event_init(destination_id);
}
/*----------------------------------------------------------------------------*/
int 
main(int argc, char **argv)
{
	int opt;
	event_t ev_new_syn;             /* New SYN UDE */
	char *fname = MOS_CONFIG_FILE;  /* path to the default mos config file */
	struct mtcp_conf mcfg;          /* mOS configuration */
#ifndef ONVM	
	int i;
#endif

	/* get the total # of cpu cores */
	g_max_cores = GetNumCPUs();       

	/* Parse command line arguments */
	while ((opt = getopt(argc, argv, "c:f:d:")) != -1) {
		switch (opt) {
		case 'f':
			fname = optarg;
			break;
		case 'c':
			if (atoi(optarg) > g_max_cores) {
				printf("Available number of CPU cores is %d\n", g_max_cores);
				return -1;
			}
			#ifdef ONVM
			g_run_core = atoi(optarg);
			#else
			g_max_cores = atoi(optarg);
			#endif
			break;
		case 'd':
			destination_id = atoi(optarg);
			printf("destination_id:%d\n",destination_id);
			break;
		default:
			printf("Usage: %s [-f mos_config_file] [-c #_of_cpu]\n", argv[0]);
			return 0;
		}
	}

	/* parse mos configuration file */
	if (mtcp_init(fname)) {
		fprintf(stderr, "Failed to initialize mtcp.\n");
		exit(EXIT_FAILURE);
	}

	/* set the core limit */
	mtcp_getconf(&mcfg);
	#ifdef ONVM
	mcfg.num_cores = 1;
	#else
	mcfg.num_cores = g_max_cores;
	#endif
	mtcp_setconf(&mcfg);

	/* Register signal handler */
	mtcp_register_signal(SIGINT, sigint_handler);

#ifdef ONVM
	#if 0
	int retval = init_pubsub_event_msg_pool();
	if (retval != 0) {
        rte_exit(EXIT_FAILURE, "Cannot create pubsub event message pool: %s\n", rte_strerror(rte_errno));
    }
	retval = init_pubsub_event_send_msg_pool();
	if (retval != 0) {
        rte_exit(EXIT_FAILURE, "Cannot create pubsub event send message pool: %s\n", rte_strerror(rte_errno));
    }
	retval = init_pubsub_msg_pool();
    if (retval != 0) {
                rte_exit(EXIT_FAILURE, "Cannot find pubsub message pool: %s\n", rte_strerror(rte_errno));
    }
	#endif
	int retval = init_pubsub_msg_pool();
    if (retval != 0) {
        rte_exit(EXIT_FAILURE, "Cannot create pubsub message pool: %s\n", rte_strerror(rte_errno));
    }
	pubsub_msg_pool = lookup_pubsub_msg_pool();
	if(pubsub_msg_pool == NULL)
		exit(-1);
	else{
		printf("Get pubsub_msg_pool...\n");
	}
	//retval = lookup_pubsub_msg_pool();
	//send_event_mempool(destination_id);
	//send_event_msg_pool(destination_id);
	//send_event_send_msg_pool(destination_id);

	
	printf("ONVM is enabled!\n\n");
	if (!(g_mctx[g_run_core] = mtcp_create_context(g_run_core))) {
		fprintf(stderr, "Failed to craete mtcp context.\n");
		return -1;
	}
	/* init monitor */
	InitMonitor(g_mctx[g_run_core], ev_new_syn);
	
	/* wait until mOS finishes */
	mtcp_app_join(g_mctx[g_run_core]);
	
#else
	printf("ONVM is disabled!\n\n");
	for (i = 0; i < g_max_cores; i++) {
		/* Run mOS for each CPU core */
		if (!(g_mctx[i] = mtcp_create_context(i))) {
			fprintf(stderr, "Failed to craete mtcp context.\n");
			return -1;
		}
		/* init monitor */
		InitMonitor(g_mctx[i], ev_new_syn);
	}

	/* wait until mOS finishes */
	free_pubsub_msg_pool();
	
	for (i = 0; i < g_max_cores; i++)
		mtcp_app_join(g_mctx[i]);
#endif

	
	mtcp_destroy();
	return 0;
}
/*----------------------------------------------------------------------------*/

