#include "pingback.h"
#include "dev/cc2420.h"

#define RSSI 1
#define SINK_NODE 5
#define NUM_HISTORY_ENTRIES 4
#define MAX_RETRANSMISSIONS 4
/*---------------------------------------------------------------------------*/
PROCESS(pingback_process, "Pingback Process");

//PROCESS(open_connection_process, "Open Connection Process");

PROCESS(sink_handler_process, "Sink Handler Process");
SHELL_COMMAND(ping_command,
              "ping-node",
              "ping-node: pings the given node, only used by matlab",
              &sink_handler_process);

PROCESS(ping_init_process, "Ping init process");
AUTOSTART_PROCESSES(&ping_init_process);

/*---------------------------------------------------------------------------*/
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
		process_start(&pingback_process, NULL);
		packetbuf_clear();
	} else {
		char received_string[10];
		strcpy(received_string, packetbuf_dataptr());
		uint8_t counter = 0;
		for (counter = 0; counter < 10; counter++)
			if (received_string[counter] == '!') break;
		for (counter; counter < 10; counter++)
			received_string[counter] = '\0';
		#if RSSI	
		printf("RSSI = %d\nTX_POWER=%s\n\n", packetbuf_attr(PACKETBUF_ATTR_RSSI), received_string);
		#else
		printf("DATA %d %d %s\n", from->u8[0], cur_time, received_string);
		#endif	
		packetbuf_clear();
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
/*PROCESS_THREAD(open_connection_process, ev, data)
{
	PROCESS_BEGIN();
	
	open_runicast();
	
	PROCESS_END();
}*/

PROCESS_THREAD(ping_init_process, ev, data)
{
	PROCESS_BEGIN();
	
	serial_shell_init();
	shell_blink_init();
	shell_reboot_init();	
	shell_rime_init();
//	shell_rime_netcmd_init();
	shell_text_init();
	shell_time_init();
	shell_sky_init();
	shell_pingback_matlab_init();

	PROCESS_END();
}

PROCESS_THREAD(pingback_process, ev, data)
{
	PROCESS_BEGIN();
	
	static struct etimer etimer;
	static char message[4];
	static char power_str[4];
	static uint8_t power_level[8] = {31, 27, 23, 19, 15, 11, 7, 3};

	#if RSSI
	
	static int i = 0;
	leds_on(LEDS_ALL);
	for (i = 0; i < 8; i++)
	{
		cc2420_set_txpower(power_level[i]);
		itoa(power_level[i], power_str, 10);
		strcat(power_str, "!\0");
		transmit_runicast(power_str, SINK_NODE);
		etimer_set(&etimer, CLOCK_SECOND/2);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	}
	leds_off(LEDS_ALL);
	
	#else	
	
	etimer_set(&etimer, CLOCK_SECOND/16);
	sensor_init();
	leds_on(LEDS_ALL);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	itoa(sensor_read(), message, 10);
	sensor_uinit();
	strcat(message, "!\0");
	
	transmit_runicast(message, SINK_NODE);
	leds_off(LEDS_ALL);
	
	#endif

	PROCESS_END();
}

PROCESS_THREAD(sink_handler_process, ev, data)
{
	PROCESS_BEGIN();
	
	uint8_t next_node = atoi(strtok(data, " "));
	transmit_runicast("PING", next_node);

	PROCESS_END();
}

/*---------------------------------------------------------------------------*/

void shell_pingback_matlab_init()
{
	open_runicast();
	shell_register_command(&ping_command);
}
