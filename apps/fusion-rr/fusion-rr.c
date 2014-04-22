#include "fusion-rr.h" 

#define FIRST_NODE 9
#define LAST_NODE 20
#define SINK_NODE 23
#define DIR_NODE 24
#define FREQUENCY 15
#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 4

/*---------------------------------------------------------------------------*/
PROCESS(round_robin_blink_process, "rr-blink");

PROCESS(change_dir_process, "change-dir");

PROCESS(shell_round_robin_start_process, "rr-start");
SHELL_COMMAND(round_robin_start_command,
              "rr-start",
			  "rr-start: starts the round-robin lights",
			  &shell_round_robin_start_process);

PROCESS(shell_round_robin_end_process, "rr-end");
SHELL_COMMAND(round_robin_end_command,
              "rr-end",
			  "rr-end: ends the round-robin lights",
			  &shell_round_robin_end_process);
/*---------------------------------------------------------------------------*/
static char *received_string;
static uint8_t healing_offset = 0;
LIST(history_table);
MEMB(history_mem, struct history_entry, NUM_HISTORY_ENTRIES);


static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
	struct history_entry *e = NULL;
	for (e = list_head(history_table); e != NULL; e = e->next)
	{
		if (rimeaddr_cmp(&e->addr, from))
			break;
	}
	if (e == NULL)
	{
		e = memb_alloc(&history_mem);
		if (e == NULL)
			e = list_chop(history_table);
		rimeaddr_copy(&e->addr, from);
		e->seq = seqno;
		list_push(history_table, e);
	}
	else
	{
		if (e->seq == seqno)
		{
			printf("Runicast message received from %d.%d, seqno %d (DUPLICATE)\n",
			       from->u8[0], from->u8[1], seqno);
			return;
		}
		e->seq = seqno;
	}

	printf("Runicast message received from %d.%d: %s\n",
	       from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

	/* Receiving a message triggers the next process in the sequence to begin */
	received_string = (char *)packetbuf_dataptr();
	if (rimeaddr_node_addr.u8[0] != SINK_NODE)
		process_start(&round_robin_blink_process, NULL);
}

static void
sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	printf("Runicast message sent to %d.%d, retransmissions %d\n",
	       to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	printf("Runicast message timed out when sending to %d.%d, retransmissions %d\n",
	       to->u8[0], to->u8[1], retransmissions);
}

static const struct runicast_callbacks runicast_callbacks = {
                         recv_runicast,
						 sent_runicast,
						 timedout_runicast};

static struct runicast_conn runicast;

int transmit_runicast(char *message, uint8_t addr_one)
{
	rimeaddr_t recv;
	packetbuf_copyfrom(message, strlen(message));
	recv.u8[0] = addr_one;
	recv.u8[1] = 0;
	packetbuf_copyfrom(message, strlen(message));
	return runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
}

void open_runicast(void)
{
	list_init(history_table);
	memb_init(&history_mem);
	runicast_open(&runicast, 144, &runicast_callbacks);
}

void close_runicast(void)
{
	runicast_close(&runicast);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(rr_start_process, ev, data)
{
	if (rimeaddr_node_addr.u8[0] == DIR_NODE)
		process_start(&change_dir_process,NULL);
	
	if (rimeaddr_node_addr.u8[0] == FIRST_NODE)
		static struct etimer etimer;
		static uint8_t next_node = 0;
		if (rimeaddr_node_addr.u8[0] == 12)
			next_node = 14;
		if (rimeaddr_node_addr.u8[0] == LAST_NODE)
			next_node = FIRST_NODE;

		leds_on(LEDS_ALL);
		etimer_set(&etimer, CLOCK_SECOND/FREQUENCY);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
		transmit_runicast 	

/*---------------------------------------------------------------------------*/
void shell_fusion_rr_init(void)
{
	open_runicast();
	open_unicast();
	shell_register_command(&round_robin_start_command);
	shell_register_command(&round_robin_end_command);
}
/*---------------------------------------------------------------------------*/
