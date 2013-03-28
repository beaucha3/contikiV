/* This program localizes a [gaussian] source using individual, independent sensor
 * readings, which are sent to a fusion center for calculation.
 * Authors: Dario Aranguiz and Kyle Harris
 */


#include "grad-desc-localization.h"

#define FIRST_NODE 9
#define LAST_NODE 15
#define SINK_NODE 4
#define NUM_HISTORY_ENTRIES 4
#define MAX_RETRANSMISSIONS 4

/*---------------------------------------------------------------------------*/
PROCESS(shell_grad_desc_localization_start_process, "localize");
SHELL_COMMAND(grad_desc_command,
              "start-localize",
			  "start-localize: begins grad-desc localization",
			  &shell_grad_desc_localization_start_process);

PROCESS(sine_test, "sine");
SHELL_COMMAND(sine_command,
              "sine-test",
              "sine-test: tests sin/cos(53)",
              &sine_test);

PROCESS(sink_localization_selfstart_process, "Sink helper process");

PROCESS(node_data_gather_process, "Node data-gather process");
/*---------------------------------------------------------------------------*/
static char *received_string; //[(LAST_NODE - FIRST_NODE + 1) * 4];
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
		process_start(&node_data_gather_process, NULL);
	else process_start(&sink_localization_selfstart_process, NULL);
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
PROCESS_THREAD(shell_grad_desc_localization_start_process, ev, data)
{
	PROCESS_BEGIN();

	open_runicast();

	if (rimeaddr_node_addr.u8[0] == FIRST_NODE)
	{
		static struct etimer etimer0;
		uint8_t next_node = FIRST_NODE + 1;
		uint16_t sensor_value;
		static char message[3];

		etimer_set(&etimer0, CLOCK_SECOND/16);
		sensor_init();
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer0));
		sensor_value = sensor_read();
		sensor_uinit();
		itoa(sensor_value, message, 10);

		leds_on(LEDS_ALL);
		transmit_runicast(message, next_node);
		leds_off(LEDS_ALL);
	}	

	PROCESS_END();
}

PROCESS_THREAD(node_data_gather_process, ev, data)
{
	PROCESS_BEGIN();

	static struct etimer etimer;
	static uint8_t my_node;
	static uint8_t next_node;
	static char message[4];
	my_node = rimeaddr_node_addr.u8[0];
	next_node = my_node + 1;
	if (my_node == LAST_NODE)
		next_node = SINK_NODE;

	static uint16_t sensor_value;
	etimer_set(&etimer, CLOCK_SECOND/16);
	sensor_init();
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	sensor_value = sensor_read();
	sensor_uinit();

	itoa(sensor_value, message, 10);
	strcat(received_string, " ");
	strcat(received_string, message);

	leds_on(LEDS_ALL);
	transmit_runicast(received_string, next_node);
	leds_off(LEDS_ALL);

	PROCESS_END();
}

PROCESS_THREAD(sink_localization_selfstart_process, ev, data)
{
	PROCESS_BEGIN();

	static uint16_t readings[LAST_NODE - FIRST_NODE + 1];
	static uint8_t i = 0;
	static char *parsed_value;
	
	parsed_value = strtok(received_string, " ");
	for (i = 0; i <= (LAST_NODE - FIRST_NODE); i++)
	{
		readings[i] = atoi(parsed_value);
		parsed_value = strtok(NULL, " ");
	}

	/* Calculations */
	
	transmit_runicast("\0", FIRST_NODE);

	PROCESS_END();
}

PROCESS_THREAD(sine_test, ev, data)
{
	PROCESS_BEGIN();

	static signed short sin_value = 0;
	static signed short cos_value = 0;
	static signed short log_value = 0;
	sin_value = qsin(53);
	cos_value = qcos(53);
	log_value = qlog(53);

	printf("Sine of 53 = %d\nCosine of 53 = %d\nLog of 655 = %d\n",
			sin_value, cos_value, log_value);
	
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void shell_grad_desc_localization_init()
{
	open_runicast();
	shell_register_command(&grad_desc_command);
	shell_register_command(&sine_command);
}

/* General steps for mote adaptation:
 * - Get sensor reading
 * - Concatenate it with the already present data string 
 * --- *Note, looks like this "100 101 483" etc
 * - Pass on new data string to next node
 * - At end, pass final data string to sink node
 * 
 * Sink node (start process in the init function so it runs on sink?):
 * - Parse string and pass data individually into array
 * - Calculations
 * - Flag iteration as complete
 * - Send command to continue data collection to the first node
 */
