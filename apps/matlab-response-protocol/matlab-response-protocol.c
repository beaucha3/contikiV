/* This program localizes a [gaussian] source using individual, independent sensor
 * readings, which are sent to a fusion center for calculation.
 * Authors: Dario Aranguiz and Kyle Harris
 */


#include "matlab-response-protocol.h"
#include "stdio.h"

#define FIRST_NODE 9 
#define LAST_NODE 15
#define SINK_NODE 1 
#define SLEEP_TIMEOUT 20
#define MAX_RETRANSMISSIONS 1

/*---------------------------------------------------------------------------*/
PROCESS(node_read_process, "Node read process");

PROCESS(sink_handler_process, "Sink handler process");
SHELL_COMMAND(sink_handler_command,
              "sink-send",
              "sink-send: used only by matlab - 'sink-send SLEEP_TIME NODE_ID'",
              &sink_handler_process);
/*---------------------------------------------------------------------------*/
static uint16_t sleep_time = 0;
static char *node_received_string = "\0";

static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{	
	uint16_t cur_time = clock_time()/CLOCK_SECOND;

	//printf("Node receives: %d\n%d\n%s\n", from->u8[0], cur_time, (char *)packetbuf_dataptr());

	/* Receiving a message triggers the next process in the sequence to begin */
	
	if (rimeaddr_node_addr.u8[0] != SINK_NODE)
	{
		node_received_string = (char *)packetbuf_dataptr();
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
		packetbuf_clear();
	}
}

static void
sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	//printf("Runicast to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	//printf("Runicast message timed out when sending to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}

static const struct runicast_callbacks runicast_callbacks = {
                         recv_runicast,
						 sent_runicast,
						 timedout_runicast};

static struct runicast_conn runicast;

void transmit_runicast(char *message, uint8_t addr_one)
{
	rimeaddr_t recv;
	packetbuf_copyfrom(message, strlen(message));
	recv.u8[0] = addr_one;
	recv.u8[1] = 0;
	if(!runicast_is_transmitting(&runicast)) {
		//printf("%d\n",recv.u8[0]);
		runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
	}
}

void open_runicast(void)
{
	runicast_open(&runicast, 144, &runicast_callbacks);
}

void close_runicast(void)
{
	runicast_close(&runicast);
}

/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
	node_received_string = (char *)packetbuf_dataptr();
	process_start(&node_read_process, NULL);
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

static struct broadcast_conn broadcast;

int transmit_broadcast(char *message)
{
	packetbuf_copyfrom(message, strlen(message));
	return broadcast_send(&broadcast);
}

void open_broadcast(void)
{
	broadcast_open(&broadcast, 129, &broadcast_call);
}

void close_broadcast(void)
{
	broadcast_close(&broadcast);
}

/*---------------------------------------------------------------------------*/
int16_t parse_sleep_value()
{
	/* String received will either be of the form "SINGLE sleep_time" or
     * "VECTOR NODE 1 2 3 SLEEP 0 2 12"
	 * From this, we want to pull the proper sleep information for the sensor
	 */
	int sleep_time = 0;
	
	// Set up a mess around string node_string2 which we can use instead of node_received_string.
	char *node_string2;
	node_string2 = (char *) malloc(strlen(node_received_string) + 1);
	strcpy(node_string2, node_received_string);
	
	// DEBUG CODE
	//printf("before token node_string2 is: %s\n", node_string2);

	// Tokenize up to the suffix.
	if(strtok(node_string2, "\n")==NULL) {
		// If no suffix found then FAILURE! Return sleep_time as -1.
		sleep_time = -1;
	} else {
		// We have the suffix... proceed.
		
		// DEBUG CODE
		//printf("Parsing this: %s\n", node_string2);

		if (!strcmp(strtok(node_string2, " "), "SINGLE")) {
			// Handle the SINGLE case
			sleep_time = atoi(strtok(NULL, " "));
		} else {
			// Handle the VECTOR case.
			char *cur_string;
			cur_string = strtok(NULL, " "); // Returns "NODE".
			cur_string = strtok(NULL, " "); // Returns first node id.
			int8_t string_counter = 0;
			int8_t my_id_counter = -1;
			
			// DEBUG CODE
			//printf("cur_string is: %s\n", cur_string);

			while (strcmp(cur_string, "SLEEP") != 0) {
				// Iterate through all the node ids, and store the index of THIS sensor,
				// if it is present.
	
				if (atoi(cur_string) == rimeaddr_node_addr.u8[0]) {
					// If this sensor is listed in the VECTOR command, store its index.
					my_id_counter = string_counter;
				}
				string_counter++;
				cur_string = strtok(NULL, " ");
			}
			
			sleep_time = -1;
			if (my_id_counter != -1) {
				// If THIS sensor is being addressed in the VECTOR command.
				
				// DEBUG CODE
				//printf("This sensor is being addressed!\n");

				for (my_id_counter; my_id_counter >= 0; my_id_counter--)
					// Tokenize through the sleep times until we're at the one relating
					// to THIS sensor.
					cur_string = strtok(NULL, " ");
				sleep_time = atoi(cur_string);
			}
		}
	}
	free(node_string2);
	return sleep_time;
}

PROCESS_THREAD(node_read_process, ev, data)
{
	PROCESS_BEGIN();

	sleep_time = parse_sleep_value();

	if (sleep_time != -1)
	{	
		static struct etimer etimer;
		static int16_t sensor_value = 0;
		static char message[3];
		
		etimer_set(&etimer, CLOCK_SECOND * sleep_time);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	
		etimer_set(&etimer, CLOCK_SECOND/16);
		leds_on(LEDS_ALL);
		sensor_init();
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
		sensor_value = sensor_read();
		sensor_uinit();
		itoa(sensor_value, message, 10);
		strcat(message, "!\0");
	
		transmit_runicast(message, SINK_NODE);
		leds_off(LEDS_ALL);
	}

	packetbuf_clear();

	PROCESS_END();
}

/*Sends the sleep time to a certain node
 *Syntax is either "sink-send SINGLE sleep_time node_id"
 *Ex. "sink-send SINGLE 200 12"	
 *Or
 *"sink-sind VECTOR NODE {node_id's} SLEEP {sleep_times}"
 *Ex. "sink-send VECTOR NODE 1 2 3 SLEEP 12 5 10" 
 */
PROCESS_THREAD(sink_handler_process, ev, data)
{
	PROCESS_BEGIN();

	char *input_string;
	input_string = (char *) malloc(strlen(data) + 1);
	strcpy(input_string, data);

	if (!strcmp(strtok(data, " "), "SINGLE"))
	{
		// Single handler
		uint8_t next_node = atoi(strtok(NULL, " "));
		char message[20] = "SINGLE \0";
		char *sleep = strtok(NULL, " ");
		strcat(message, sleep);
		transmit_runicast(message, next_node);
	}
	else
	{
		// Vector handler
		// Add suffix to get rid of repeated value problem
		input_string = strcat(input_string, "\n");
		transmit_broadcast(input_string);
	}

	free(input_string);
	
	packetbuf_clear();
		
	PROCESS_END();
}	

/*---------------------------------------------------------------------------*/
void shell_matlab_response_protocol_init()
{
	open_runicast();
	//open_unicast();
	open_broadcast();
	shell_register_command(&sink_handler_command);
}


/*---------------------------------------------------------------------------*/
/*
 * Deprecated Processes


PROCESS(node_timeout_process, "Node timeout process");

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

PROCESS(shell_sleepy_trilat_start_process, "Sleepy-Trilat Start Process");
SHELL_COMMAND(sleepy_trilat_command,
              "strilat",
			  "strilat: begins sleepy tracking and trilateration",
			  &shell_sleepy_trilat_start_process);


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
*/
