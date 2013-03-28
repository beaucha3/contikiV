/* This program sequentially collects sensor readings and passes on a cumulative
 * average, beginning with the first node listed. At the end of the sequence,
 * the cumulative average is sent to the sink node, and the sequence starts over.
 * The nodes are referenced in the style of a singly-linked list, with only the first
 * and last nodes aware of their absolute position in the list, and the rest only
 * aware of the node directly succeeding them..
 * Author: Dario Aranguiz
 */

#include "rr-trans.h"

#define FIRST_NODE 9
#define LAST_NODE 20
#define SINK_NODE 23
#define FREQUENCY 15
#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 4

/*---------------------------------------------------------------------------*/
PROCESS(round_robin_blink_process, "rr-blink");

PROCESS(shell_round_robin_start_process, "rr-start");
SHELL_COMMAND(round_robin_start_command,
              "rr-start",
			  "rr-start: starts the round-robin collection",
			  &shell_round_robin_start_process);

PROCESS(shell_round_robin_end_process, "rr-end");
SHELL_COMMAND(round_robin_end_command,
              "rr-end",
			  "rr-end: ends the round-robin collection",
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

/* This process initializes the sequence at the first node, but only ever runs
 * once during a given session of data collection.
 */
PROCESS_THREAD(shell_round_robin_start_process, ev, data)
{
	PROCESS_BEGIN();
	
	open_runicast();
	open_unicast();

	/* Data collection always begins with the first node */
	if (rimeaddr_node_addr.u8[0] == FIRST_NODE)
	{ 
		/* Initialization of the first node's ID and neighboring node */
		static struct etimer etimer0;
		uint8_t next_node = FIRST_NODE + 1;
		uint16_t sensor_value;
		static char message[5];

		/* Collects data from the light sensor and converts it to a string */
		etimer_set(&etimer0, CLOCK_SECOND/16);
		sensor_init();
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer0));
		sensor_value = sensor_read();
		sensor_uinit();
		itoa(sensor_value, message, 10);

		/* Sends data to the second node in the sequence */
  //  	etimer_set(&etimer0, CLOCK_SECOND/FREQUENCY);
		leds_on(LEDS_ALL);
	//	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer0));
		transmit_runicast(message, next_node);
		leds_off(LEDS_ALL);
	}
	
	PROCESS_END();
}

/* This process performs the brunt of the data transmission, as well as
 * the data collection and the calculation of the average. Instead of 
 * using logical loops, a recursive approach is taken, with each
 * prior process calling the next process, and the prior process terminating
 * itself after the next process is called.
 */
PROCESS_THREAD(round_robin_blink_process, ev, data)
{
	PROCESS_BEGIN();

	/* Initialization of node ID's and neighbor's node ID */
	static struct etimer etimer;
	static uint8_t my_node;
	static char message[5];
	my_node = rimeaddr_node_addr.u8[0];
	static uint8_t next_node;
	next_node = my_node + 1;
	if (my_node == LAST_NODE)
		next_node = FIRST_NODE;
	static uint8_t sent = 0;

	/* Sensor data is collected and the received message is converted to an integer */
	static uint16_t new_data;
	static uint16_t received_data;
	received_data = atoi(received_string);
	etimer_set(&etimer, CLOCK_SECOND/16);
	sensor_init();
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	new_data = sensor_read();
	sensor_uinit();
		
	/* Cumulative average is calculated and converted back to a string */
	received_data = received_data * (my_node - FIRST_NODE - healing_offset);
	new_data = new_data + received_data;
	new_data = new_data / (my_node - FIRST_NODE + 1 - healing_offset);
	itoa(new_data, message, 10);

	/* Message is sent to the next node, and the sink node if it is the last node */
//	etimer_set(&etimer, CLOCK_SECOND/FREQUENCY);
	leds_on(LEDS_ALL);
//	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	if (my_node == LAST_NODE)
	{
		transmit_unicast(message, SINK_NODE);
		transmit_runicast("0", next_node);
	} 
	else
	{
		transmit_runicast(message, next_node);
	/*	while (sent != 1)
		{

			if (transmit_runicast(message, next_node))
			{
				transmit_unicast("Successful transmission", SINK_NODE);
				sent = 1;
			}
			else if (next_node = LAST_NODE)
			{
				transmit_unicast("Last node has failed, ending sequence", SINK_NODE);
				process_start(&shell_round_robin_end_process, NULL);
			}
			else if (next_node = FIRST_NODE)
			{
				transmit_unicast("First node has failed, ending sequence", SINK_NODE);
				process_start(&shell_round_robin_end_process, NULL);
			}
			else
			{
				next_node++;
				healing_offset++;
				sent = 0;
			}
		} */
	}
				

	leds_off(LEDS_ALL);

	PROCESS_END();
}

/* This process simply terminates the session */
PROCESS_THREAD(shell_round_robin_end_process, ev, data)
{
	PROCESS_BEGIN();

	close_unicast();
	close_runicast();
	process_exit(&round_robin_blink_process);
	
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void shell_rr_trans_init(void)
{
	open_runicast();
	open_unicast();
	shell_register_command(&round_robin_start_command);
	shell_register_command(&round_robin_end_command);
}
/*---------------------------------------------------------------------------*/
