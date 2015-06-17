#include "contiki.h" /* Main contiki library */
#include "shell.h"
#include "serial-shell.h"
#include "lib/sensors.h"
#include "lib/memb.h"
#include "lib/list.h"
#include "net/rime.h" /* Communication stack */
#include "dev/leds.h" /* Libraries for sensors */
#include "dev/button-sensor.h"
#include "dev/sky-sensors.h"
#include "dev/temperature-sensor.h"
#include "dev/sht11-sensor.h"
#include "dev/light-sensor.h"
#include "sys/etimer.h"
#include "node-id.h"
#include "dev/sht11-arch.h" /* Architecture-specific definitions for the temperature/humidity sensor */
#include <stdio.h> /* Standard C libraries for i/o, math, strings, etc. */
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>

#define DEBUG 0

PROCESS(collect_process, "Collect process");
AUTOSTART_PROCESSES(&collect_process);

/* Set up communications */

static struct broadcast_conn bc;

static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) /* Print value if broadcast is received */
{
	static float x;
	static int dec;
	static float frac;

	packetbuf_copyto(&x);

	dec = x;
	frac = x - dec;

	printf("Message from %d: Humidity = %d.%02u %\n", from->u8[0], dec, (unsigned int)(frac * 100));
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

PROCESS_THREAD(collect_process, ev, data)
{
	static struct etimer et;

	PROCESS_EXITHANDLER(broadcast_close(&bc);)

	PROCESS_BEGIN();

	broadcast_open(&bc, 129, &broadcast_call); /* Open broadcast */

	while(1)
	{
		etimer_set(&et, 2*CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		#if DEBUG

			printf("loop\n");

		#endif

		etimer_reset(&et);
	}

	PROCESS_END();
}




