/*

File: One of the main source files for the summer 2015 air quality monitoring project.
When compiled, this file is meant to be loaded onto the sniffer node that receives brodcasts from the main network.


Author: Nathan Beauchamp (beaucha3@illinois.edu)

with assistance from Isaac Kousari (kousari2@illinois.edu)

Institution: University of Illinois at Urbana-Champaign, Coordinated Science Lab
Affiliation: Undergraduate Research Assistants under Professor Veeravalli
Special Thanks to: Neeraj Venkatesan, UIUC M.S. EE graduate

*/

#define DEBUG 1

#include "air-quality.h" /* All includes, structure definitions, and macros are in this file- refer to it */

PROCESS(sniffer_process, "Sniffer process");
PROCESS(wait_process, "Wait process");
AUTOSTART_PROCESSES(&sniffer_process);

/* Set up communications */

static void broadcast_recv(struct broadcast_conn *bc, const rimeaddr_t *from) /* Executes if bc is received */
{
	static data_message_t message; /* Temporary variable to store received data */

	packetbuf_copyto(&message); /* Store contents of packet buffer */

	printf("Message received from %d.%d:\n", from->u8[0], from->u8[1]); /* Print the message */
	printf("The message key is %d, the temperature value is %d, and the humidity value is %d\n", message.key, message.temp, message.humid);
}

/* This is where we specify that broadcast_recv should be called when a broadcast is received.
The broadcast_call structure is passed as a pointer to broadcast_open in the main_process */

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

static struct broadcast_conn bc;

PROCESS_THREAD(sniffer_process, ev, data)
{
	static struct etimer et;

	leds_off(LEDS_ALL); /* turn off LEDs */
	SENSORS_ACTIVATE(button_sensor); /* turn on button sensor */

	PROCESS_EXITHANDLER(broadcast_close(&bc));
	PROCESS_BEGIN();

	broadcast_open(&bc, SNIFFER_CHANNEL, &broadcast_call);

	process_start(&wait_process, NULL); /* Start wait process.  This will end the main process when a button sensor 						event is posted */

	while(1) /* Infinite loop */
	{
		etimer_set(&et, CLOCK_SECOND * 2 * PERIOD); /* Wait 4 seconds per loop for a receive event */

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		#if DEBUG
			printf("Loop\n");
		#endif
	}


	SENSORS_DEACTIVATE(button_sensor); /* Deactivate sensor when we're done */
	PROCESS_END();
}

PROCESS_THREAD(wait_process, ev, data)
{
	PROCESS_BEGIN();

	SENSORS_ACTIVATE(button_sensor); /* activate button sensor */
	
	/* Wait and return control to main_process until button sensor is pressed */
	PROCESS_WAIT_EVENT_UNTIL((ev == sensors_event) && (data == &button_sensor));

	SENSORS_DEACTIVATE(button_sensor); /* Deactivate button sensor */
	process_exit(&sniffer_process); /* End main process once button sensor is pressed */

	PROCESS_END();
}




