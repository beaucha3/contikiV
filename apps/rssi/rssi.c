#include "rssi.h"
#include "contiki.h"
#include "shell.h"

#include "dev/leds.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include "node-id.h"

//#include "sky-transmission.h"
#include "net/rime.h"
#include "net/rime/unicast.h"
#include "net/rime/mesh.h"
#include "net/rime/runicast.h"
#include <string.h>

#define SINK_NODE 19
#define TX_NODE 24
#define RX_NODE 25

/*---------------------------------------------------------------------------*/
PROCESS(shell_rssi_measure_process, "rssi");
SHELL_COMMAND(rssi_measure_command,
              "rssi",
              "rssi: finds rssi between two nodes",
              &shell_rssi_measure_process);
/*---------------------------------------------------------------------------*/
static struct unicast_conn uc;

int transmit_unicast(char *message, uint8_t addr_one)
{
	rimeaddr_t addr;
	packetbuf_copyfrom(message, strlen(message));
	addr.u8[0] = addr_one;
	addr.u8[1] = 0;
	if (!rimeaddr_cmp(&addr, &rimeaddr_node_addr))
		return unicast_send(&uc, &addr);
	else return 0;
}

static void
recv_uc(struct unicast_conn *c, const rimeaddr_t *from)
{
	if(strcmp((char *)packetbuf_dataptr(),"rssi") == 0) {
		// If we got a rssi command then start the process.
		printf("Starting process 'rssi'...\n");
		process_start(&shell_rssi_measure_process, NULL);
	} else {
		if(rimeaddr_node_addr.u8[0] == SINK_NODE) {
			printf("%s\n",(char *)packetbuf_dataptr());
		} else if(rimeaddr_node_addr.u8[0] == RX_NODE && from->u8[0] == TX_NODE) {
			// If you are the RX_NODE, then forward packet strength to SINK_NODE.
			char message[10] = "";
			sprintf(message,"RSSI %d",packetbuf_attr(PACKETBUF_ATTR_RSSI));
			printf("%s",message);
			transmit_unicast(message, SINK_NODE);
		} else {
			printf("Unicast data received from %d.%d: %s\n",
				from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
		}
	}
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};

void open_unicast()
{
	unicast_open(&uc, 146, &unicast_callbacks);
}

void close_unicast()
{
	unicast_close(&uc);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(shell_rssi_measure_process, ev, data)
{
	static struct etimer etimer;
	
	PROCESS_BEGIN();

	leds_off(LEDS_ALL);
	etimer_set(&etimer, CLOCK_SECOND/16);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	leds_off(LEDS_ALL);

	if(rimeaddr_node_addr.u8[0] == SINK_NODE) {
		// Code for the sink to run.
		printf("I am SINK\n");
		transmit_unicast("rssi",TX_NODE);
	} else if(rimeaddr_node_addr.u8[0] == TX_NODE) {
		// TX node code.
		printf("I am TX\n");
		transmit_unicast("I AM DERP HEAR ME ROAR!!!", RX_NODE);
	} else if(rimeaddr_node_addr.u8[0] == RX_NODE) {
		// RX node code.
		printf("I am RX\n");
		transmit_unicast("rssi",TX_NODE);
	}

	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
void shell_rssi_measure_init(void) {
	open_unicast();
	shell_register_command(&rssi_measure_command);
}
/*---------------------------------------------------------------------------*/

