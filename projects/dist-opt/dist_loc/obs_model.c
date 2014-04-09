/*
 * obs_model.c
 * 
 * Getting model data for distributed light source localization.  
 * 
 * The mote will get a reading from the S1087 sensor and will broadcast the result.
 * 
 */

#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"
#include "dev/light-sensor.h"
#include "dev/button-sensor.h"
#include "obs_model.h"

#define NUM_DATA = 50;

static struct broadcast_conn broadcast;
	
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};


PROCESS(main_process, "main");
AUTOSTART_PROCESSES(&main_process);

PROCESS_THREAD(main_process, ev, data)
{
	static struct etimer et;
  
	PROCESS_EXITHANDLER(broadcast_close(&broadcast));
	PROCESS_BEGIN();
	 
	broadcast_open(&broadcast, COMM_CHANNEL, &broadcast_call);
	SENSORS_ACTIVATE(button_sensor);
	
	// Don't start data collection until user button is pressed
	PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);

	
	// Wait two seconds after activating light sensor to allow user to move away after 
	// pressing the button
	SENSORS_ACTIVATE(light_sensor);
	etimer_set(&et, CLOCK_SECOND*2);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		
	int16_t out; 
	int i; 

	// Broadcast NUM_DATA light measurements and stop
	for( i = 0; i < NUM_DATA; i++)
	{
		etimer_set(&et, CLOCK_SECOND/NUM_DATA);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		
		out = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
		
		packetbuf_copyfrom( &out,sizeof(out) );
		broadcast_send(&broadcast);
	}

	PROCESS_END();
}

