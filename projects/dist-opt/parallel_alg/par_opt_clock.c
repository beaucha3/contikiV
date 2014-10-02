/* Code for Master Clock Node that signals the beginning 
 * of each optimization round every few seconds.
 * 
 * Clock Period is configurable with the 
 */


#include "contiki.h"
#include "net/rime.h"
#include <stdio.h>
#include "dev/leds.h"
#include "par_opt.h"

#define PERIOD CLOCK_SECOND*10

/*---------------------------------------------------------------------------*/
PROCESS(broadcast_clock_process, "Broadcast Clock");
AUTOSTART_PROCESSES(&broadcast_clock_process);
/*---------------------------------------------------------------------------*/
static struct broadcast_conn broadcast;

static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from){}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_clock_process, ev, data)
{
  static struct etimer et;
  static clock_message_t out;
  out.key = CKEY;

  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();

  broadcast_open(&broadcast, CLOCK_CHANNEL, &broadcast_call);
  
  SENSORS_ACTIVATE(button_sensor);
    
  // Don't start clock signal until user button is pressed
  PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event 
                        && data == &button_sensor);
  
  while(1) 
  {
    // Delay by clock period 
    etimer_set(&et, PERIOD);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // Broadcast clock message
    packetbuf_copyfrom( &out,sizeof(out) );
    broadcast_send(&broadcast);
    
    // Blink Green LEDs to indicate we just sent out a clock message 
    leds_on( LEDS_GREEN );
    
    etimer_set(&et, CLOCK_SECOND / 8 );
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
   
    leds_off( LEDS_GREEN );  
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

