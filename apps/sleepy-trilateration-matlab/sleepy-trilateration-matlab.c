/* This program localizes a [gaussian] source using individual, independent sensor
 * readings, which are sent to a fusion center for calculation.
 * Authors: Dario Aranguiz and Kyle Harris
 */


#include "sleepy-trilateration-matlab.h"

#define FIRST_NODE 9 
#define LAST_NODE 15
#define SINK_NODE 1 
#define SLEEP_TIMEOUT 20
#define NUM_HISTORY_ENTRIES 4
#define MAX_RETRANSMISSIONS 4

/*---------------------------------------------------------------------------*/
PROCESS(shell_sleepy_trilat_start_process, "Sleepy-Trilat Start Process");
SHELL_COMMAND(sleepy_trilat_command,
              "strilat",
			  "strilat: begins sleepy tracking and trilateration",
			  &shell_sleepy_trilat_start_process);

PROCESS(node_read_process, "Node read process");

PROCESS(sink_handler_process, "Sink handler process");
SHELL_COMMAND(sink_handler_command,
              "sink-send",
              "sink-send: used only by matlab - 'sink-send SLEEP_TIME NODE_ID'",
              &sink_handler_process);

PROCESS(node_timeout_process, "Node timeout process");
/*---------------------------------------------------------------------------*/
LIST(history_table);
MEMB(history_mem, struct history_entry, NUM_HISTORY_ENTRIES);
static uint8_t my_node = 0;
static uint16_t sleep_time = 0;
static uint16_t my_noise = 0;

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
		/*	printf("Runicast message received from %d.%d, seqno %d (DUPLICATE)\n",
			       from->u8[0], from->u8[1], seqno);
		*/	return; 
		}
		e->seq = seqno;
	}
	
	uint16_t cur_time = clock_time()/CLOCK_SECOND;

//	printf("%d\n%d\n%s\n", from->u8[0], cur_time, (char *)packetbuf_dataptr());

	/* Receiving a message triggers the next process in the sequence to begin */
	
	if (rimeaddr_node_addr.u8[0] != SINK_NODE)
	{
		sleep_time = atoi(packetbuf_dataptr());
		process_exit(&node_timeout_process);
		process_start(&node_read_process, NULL);
	} else {
		char received_string[10];
		strcpy(received_string, packetbuf_dataptr());
		uint8_t counter = 0;
		for (counter = 0; counter < 10; counter++)
			if (received_string[counter] == '!') break;
		for (counter; counter < 10; counter++)
			received_string[counter] = '\0';
		printf("DATA %d %d %s\n", from->u8[0], cur_time, received_string);
	}
}

static void
sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
/*	printf("Runicast message sent to %d.%d, retransmissions %d\n",
	       to->u8[0], to->u8[1], retransmissions); */
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

PROCESS_THREAD(shell_sleepy_trilat_start_process, ev, data)
{
	PROCESS_BEGIN();

	open_runicast();
	my_node = rimeaddr_node_addr.u8[0];

	if (my_node != SINK_NODE)
	{	
		static struct etimer etimer0;
		static char message[4];
		static int i = 0;
		
		etimer_set(&etimer0, CLOCK_SECOND/16);
		sensor_init();
//		leds_on(LEDS_ALL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer0));
		for (i = 1; i <= 100; i++)
		{
			etimer_set(&etimer0, CLOCK_SECOND/50);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer0));			
			my_noise = sensor_read() + my_noise;
		}
		sensor_uinit();
		my_noise = my_noise / 100; 
		
		etimer_set(&etimer0, CLOCK_SECOND * (my_node - FIRST_NODE + 1));
		leds_on(LEDS_ALL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer0));
		leds_off(LEDS_ALL);	

		itoa(my_noise, message, 10);
		strcat(message, "!\0");

		transmit_runicast(message, SINK_NODE);

//		process_start(&node_timeout_process, NULL);
	}

	PROCESS_END();
}

PROCESS_THREAD(node_read_process, ev, data)
{
	PROCESS_BEGIN();
	
	static struct etimer etimer;
	static int16_t sensor_value = 0;
	static char message[3];
	
	etimer_set(&etimer, CLOCK_SECOND * sleep_time);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));

	etimer_set(&etimer, CLOCK_SECOND/16);
	leds_on(LEDS_ALL);
	sensor_init();
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	sensor_value = sensor_read() - my_noise;
	sensor_uinit();
	itoa(sensor_value, message, 10);
	strcat(message, "!\0");

	transmit_runicast(message, SINK_NODE);
	leds_off(LEDS_ALL);
	
//	process_start(&node_timeout_process, NULL);
	
	PROCESS_END();
}

/*Sends the sleep time to a certain node
 *Syntax is "sink-send SLEEP_TIME NODE_ID"
 *Ex. "sink-send 200 12"	
 */
PROCESS_THREAD(sink_handler_process, ev, data)
{
	PROCESS_BEGIN();

	char *next_sleep_time;
	next_sleep_time = strtok(data, " ");
	uint8_t next_node = atoi(strtok(NULL, " "));
	transmit_runicast(next_sleep_time, next_node);

	PROCESS_END();
}	

PROCESS_THREAD(node_timeout_process, ev, data)
{
	PROCESS_BEGIN();
	
	while (1)
	{
		static struct etimer timeout;
		int16_t sensor_value = 0;
		static char message[3];

		etimer_set(&timeout, CLOCK_SECOND * SLEEP_TIMEOUT);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timeout));

		etimer_set(&timeout, CLOCK_SECOND/16);
		leds_on(LEDS_ALL);
		sensor_init();
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timeout));
		sensor_value = sensor_read() - my_noise;
		sensor_uinit();
		itoa(sensor_value, message, 10);
		strcat(message, "!\0");
	
		transmit_runicast(message, SINK_NODE);
		leds_off(LEDS_ALL);
	}
	
	PROCESS_END();
}	
/*---------------------------------------------------------------------------*/
void shell_sleepy_trilateration_matlab_init()
{
	open_runicast();
	open_unicast();
	shell_register_command(&sleepy_trilat_command);
	shell_register_command(&sink_handler_command);
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
