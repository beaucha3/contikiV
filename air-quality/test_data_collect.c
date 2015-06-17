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

PROCESS(collect_process, "Collect process");
AUTOSTART_PROCESSES(&collect_process);

/* Set up communications */

static struct broadcast_conn bc;

static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) /* Do nothing if broadcast is received */
{}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

PROCESS_THREAD(collect_process, ev, data) /* Data collection process */
{
	/* Declare variables- most are for sensor measurement and conversion */

	static struct etimer et;
	static int val_t;
	static int val_h;
	static float temp_c;
	static float humid_base;
	static float humid_rel;

	PROCESS_EXITHANDLER(broadcast_close(&bc);)

	PROCESS_BEGIN();

	broadcast_open(&bc, 129, &broadcast_call); /* Open broadcast */

	printf("Starting data collection\n");

	while(1)
	{
		etimer_set(&et, 2*CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		SENSORS_ACTIVATE(sht11_sensor);

		val_t = sht11_sensor.value(SHT11_SENSOR_TEMP);
		val_h = sht11_sensor.value(SHT11_SENSOR_HUMIDITY);

		temp_c = -39.6 + 0.01*val_t;
		humid_base = -4 + 0.0405*val_h - 2.8e-6 * pow(val_h, 2);
		humid_rel = (temp_c - 25)*(0.01 + 0.00008*val_h) + humid_base;

		packetbuf_copyfrom(&humid_rel, sizeof(float));
		broadcast_send(&bc);

		printf("broadcast message sent\n");

		SENSORS_DEACTIVATE(sht11_sensor);
		etimer_reset(&et);
	}

	PROCESS_END();
}


