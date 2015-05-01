/*
 * simple-sensor.c
 * 
 * The mote will get a reading from the S1087 sensor and will broadcast the result.
 * 
 */

#include "contiki.h"
#include <stdio.h>
#include "net/rime.h"
#include "dev/leds.h"
#include "dev/light-sensor.h"
#include "dev/button-sensor.h"
#include "simple-sensor.h"

#define PERIOD 2 // Number of seconds between each broadcast
#define NODE_ID (rimeaddr_node_addr.u8[0]) // Hardcoded mote address

// Broadcast callbacks
static struct broadcast_conn broadcast;
	
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};


PROCESS(main_process, "main");
AUTOSTART_PROCESSES(&main_process);

PROCESS_THREAD(main_process, ev, data)
{
	// Turn off all LEDs
	leds_off( LEDS_ALL );
	
	// Turn on light sensor
	SENSORS_ACTIVATE(light_sensor);
	
	// Set up variables
	static struct etimer et;
	static data_message_t out;
	out.key = MKEY;
	out.id = NODE_ID; 
	
	PROCESS_EXITHANDLER(broadcast_close(&broadcast));
	PROCESS_BEGIN();
	
	// Open broadcast channel on COMM_CHANNEL as specified in simple-sensor.h
	broadcast_open(&broadcast, COMM_CHANNEL, &broadcast_call);

	// Don't start data collection until user button is pressed
    SENSORS_ACTIVATE(button_sensor);
	PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);
	
	while(1)
	{
	  // Wait for allotted time
	  etimer_set(&et, CLOCK_SECOND*PERIOD);
	  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	
	  // Poll light sensor	
	  out.data = (int64_t)light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
	
	  // Broadcast sensor data
	  packetbuf_copyfrom( &out,sizeof(out) );
	  broadcast_send(&broadcast);
	  
	  // Blink RED LED to indicate we just broadcast
	  leds_on( LEDS_RED );
    
	  etimer_set(&et, CLOCK_SECOND / 8 );
	  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
   
	  leds_off( LEDS_RED );
	}
	
	PROCESS_END();
}
