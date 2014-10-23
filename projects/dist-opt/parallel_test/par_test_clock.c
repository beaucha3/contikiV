/* Code for Master Clock Node that signals the beginning 
 * of each optimization round every few seconds.
 * 
 * Clock Period is configurable with the 
 */


#include "contiki.h"
#include "net/rime.h"
#include <stdio.h>
#include <string.h>
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "lib/memb.h"
#include "par_test.h"



/*---------------------------------------------------------------------------*/
PROCESS(runicast_clock_process, "Runicast Clock");
AUTOSTART_PROCESSES(&runicast_clock_process);
/*---------------------------------------------------------------------------*/
static struct runicast_conn runicast_clock;

/* OPTIONAL: Sender history.
 * Detects duplicate callbacks at receiving nodes.
 * Duplicates appear when ack messages are lost. */
struct history_entry
{
  struct history_entry *next;
  rimeaddr_t addr;
  uint8_t seq;
};
LIST(history_table);
MEMB(history_mem, struct history_entry, NUM_HISTORY_ENTRIES);

static void recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
   #if DEBUG > 0
     printf("runicast message received from %d.%d, seqno %d\n",
          from->u8[0], from->u8[1], seqno);
   #endif   
  
  /* Sender history */
  struct history_entry *e = NULL;
  
  for(e = list_head(history_table); e != NULL; e = e->next) 
  {
    if(rimeaddr_cmp(&e->addr, from)) 
    {
      break;
    }
  }
  
  if(e == NULL) 
  {
    /* Create new history entry */
    e = memb_alloc(&history_mem);
    
    if(e == NULL)
    {
      e = list_chop(history_table); /* Remove oldest at full history */
    }
    
    rimeaddr_copy(&e->addr, from);
    e->seq = seqno;
    list_push(history_table, e);
  } 
  else 
  {
    /* Detect duplicate callback */
    if(e->seq == seqno) 
    {
      #if DEBUG > 0
        printf("runicast message received from %d.%d, seqno %d (DUPLICATE)\n",
		  from->u8[0], from->u8[1], seqno);
      #endif 


      return;
    }
    /* Update existing history entry */
    e->seq = seqno;
  }
}

static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
   #if DEBUG > 0
     printf("runicast message sent to %d.%d, retransmissions %d\n",
          to->u8[0], to->u8[1], retransmissions);
   #endif
   
}

static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  #if DEBUG > 0
     printf("runicast message timed out when sending to %d , %d, retransmissions %d\n",
         to->u8[0], to->u8[1], retransmissions);
   #endif  
}

//static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct runicast_callbacks runicast_callbacks = 
{ 
  recv_runicast,
  sent_runicast,
  timedout_runicast
};


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_clock_process, ev, data)
{
  static struct etimer et;
  static clock_message_t out;
  out.key = CKEY;

  PROCESS_EXITHANDLER(runicast_close(&runicast_clock);)
  PROCESS_BEGIN();

  runicast_open(&runicast_clock, CLOCK_CHANNEL, &broadcast_call);
  
  SENSORS_ACTIVATE(button_sensor);
    
  // Don't start clock signal until user button is pressed
  PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event 
                        && data == &button_sensor);
  
  while(1) 
  {
    // Broadcast clock message
    packetbuf_copyfrom( &out,sizeof(out) );
    broadcast_send(&broadcast);
    
    // Blink Green LEDs to indicate we just sent out a clock message 
    leds_on( LEDS_GREEN );
    
    etimer_set(&et, CLOCK_SECOND / 8 );
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
   
    leds_off( LEDS_GREEN );
    
    // Delay by clock period 
    etimer_set(&et, PERIOD);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));  
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

