#include "pingnode.h"
#include "dev/light-sensor.h"

#define SINK_NODE 1
#define NUM_HISTORY_ENTRIES 4
#define MAX_RETRANSMISSIONS 4
/*---------------------------------------------------------------------------*/
PROCESS(pingnode_process, "Pingnode Process");
PROCESS(open_connection_process, "Open Connection Process");

AUTOSTART_PROCESSES(&open_connection_process);

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

	/* Receiving a message triggers the next process in the sequence to begin */
	
	if (rimeaddr_node_addr.u8[0] != SINK_NODE)
		process_start(&pingnode_process, NULL);
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
PROCESS_THREAD(open_connection_process, ev, data)
{
	PROCESS_BEGIN();
	
	open_runicast();
	
	PROCESS_END();
}

PROCESS_THREAD(pingnode_process, ev, data)
{
	PROCESS_BEGIN();
	
	static struct etimer etimer;
	static char message[4];
	
	etimer_set(&etimer, CLOCK_SECOND/16);
	SENSORS_ACTIVATE(light_sensor);
	leds_on(LEDS_ALL);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	itoa(light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC), message, 10);
	SENSORS_DEACTIVATE(light_sensor);
	strcat(message, "!\0");

	transmit_runicast(message, SINK_NODE);
	leds_off(LEDS_ALL);

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
