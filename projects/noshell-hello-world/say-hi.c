#include "contiki.h"
#include "random.h"

#include "dev/leds.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include "sky-transmission.h"
 
/*---------------------------------------------------------------------------*/
PROCESS(shell_broadcast_hi_process, "say-hi");
AUTOSTART_PROCESSES(&shell_broadcast_hi_process);
PROCESS_THREAD(shell_broadcast_hi_process, ev, data)
{
	static struct etimer etimer;
	
//	PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
//	PROCESS_EXITHANDLER(mesh_close(&mesh); broadcast_close(&broadcast);)
	PROCESS_BEGIN();

	static int8_t counter = 0;
	static uint8_t fail_count = 0;
	mesh_open(&mesh, 132, &callbacks);
	broadcast_open(&broadcast, 129, &broadcast_call);

	while(1) {
	counter++;

    leds_on(LEDS_ALL);
    etimer_set(&etimer, 3 * CLOCK_SECOND);
    PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
    leds_off(LEDS_ALL);

	etimer_set(&etimer, random_rand() % 5 * CLOCK_SECOND + 
		random_rand() % 100 * CLOCK_SECOND / 100);
	PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
	packetbuf_copyfrom("Hello", 6);
	broadcast_send(&broadcast);

	etimer_set(&etimer, random_rand() % 5 * CLOCK_SECOND + 
		random_rand() % 100 * CLOCK_SECOND / 100);
    PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
	char message[6] = "Hello";
	rimeaddr_t addr;
	packetbuf_copyfrom(message, sizeof(message));
	addr.u8[0] = 62;
	addr.u8[1] = 41;
	uint8_t mesh_sent = 0;
	mesh_sent = mesh_send(&mesh, &addr);
	if (mesh_sent == 0) {
		fail_count++;
		if (counter == 10 && fail_count >= 4)
		{
			mesh_open(&mesh, 132, &callbacks);
			counter = 0;
			fail_count = 0;
		} else if (counter == 10)
			counter = 0;
	}
	}

	PROCESS_END();
}
