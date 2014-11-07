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

#define START_ID  10    // ID of first node in chain
#define DEBUG 1
#define MAX_CLOCK_WAIT (MAX_RETRANSMISSIONS + 1)*4

/*---------------------------------------------------------------------------*/
PROCESS(runicast_clock_process, "Runicast Clock");
PROCESS(rx_process, "rx_proc");
AUTOSTART_PROCESSES(&runicast_clock_process);
/*---------------------------------------------------------------------------*/
static int16_t cur_cycle = 0;
static int16_t cur_node = START_ID;

static uint8_t sending_round_end = 0;
static int16_t msg_id = 1;

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

// Helper function to verify runicast recv'd message is from the current node
uint8_t is_cur_node( const rimeaddr_t* a );

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
  
  process_start(&rx_process, (char*)from);
}

static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
   msg_id = msg_id + 1;
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
   
   if(!sending_round_end)
   {
     cur_node = cur_node + 1;
   }
}

//static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct runicast_callbacks runicast_callbacks = 
{ 
  recv_runicast,
  sent_runicast,
  timedout_runicast
};


/*---------------------------------------------------------------------------*/
PROCESS_THREAD(runicast_clock_process, ev, data)
{
  static struct etimer et;
  static clock_message_t out;
  static int i;
  static int wait_count;
  static rimeaddr_t node_out;
  node_out.u8[1] = 0;
  
  static int16_t temp_node;

  PROCESS_EXITHANDLER(runicast_close(&runicast_clock);)
  PROCESS_BEGIN();

  runicast_open(&runicast_clock, COMM_CHANNEL, &runicast_callbacks);
  
  SENSORS_ACTIVATE(button_sensor);
    
  // Don't start clock signal until user button is pressed
  PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event 
                        && data == &button_sensor);
  
  while(1) 
  {
    if(cur_node == (START_ID + NUM_NODES))
    {
      sending_round_end = 1;
      
      cur_cycle = cur_cycle + 1;
      cur_node = START_ID;
  
      out.key = AKEY;
      out.cycle = cur_cycle;
      out.id = msg_id;

      for(i = 0; i < NUM_NODES; i++)
      {
        node_out.u8[0] = i + START_ID;
		  
		packetbuf_copyfrom( &out,sizeof(out) );	    
		runicast_send(&runicast_clock, &node_out, MAX_RETRANSMISSIONS);
  
		// Wait until we are done transmitting
		while( runicast_is_transmitting(&runicast_clock) )
		{
		  etimer_set(&et, CLOCK_SECOND/32);
		  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}	
	  }
	  
	  sending_round_end = 0; 
    }
      
    // Send clock message
    temp_node = cur_node;
    node_out.u8[0] = cur_node;
    
    out.key = CKEY;
    out.cycle = cur_cycle;
    out.id = msg_id;
    
    packetbuf_copyfrom( &out,sizeof(out) );

    runicast_send(&runicast_clock, &node_out , MAX_RETRANSMISSIONS);
    
    // Wait until we are done transmitting
    while( runicast_is_transmitting(&runicast_clock) )
    {
	  etimer_set(&et, CLOCK_SECOND/32);
	  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
    
    // Blink Green LEDs to indicate we just sent out a clock message 
    leds_on( LEDS_GREEN );
    
    etimer_set(&et, CLOCK_SECOND / 8 );
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
   
    leds_off( LEDS_GREEN );
    
    wait_count = 0;    
    while(temp_node == cur_node)
    {
	  etimer_set(&et, CLOCK_SECOND);
	  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	  
	  wait_count = wait_count + 1;
	  
	  if(wait_count == MAX_CLOCK_WAIT)
	  {
		  break;
	  }	  
    }  
  }

  PROCESS_END();
}

PROCESS_THREAD(rx_process, ev, data)
{
  PROCESS_BEGIN();
    
  static opt_message_t msg;
  static struct etimer et;
  
  packetbuf_copyto(&msg);
  
  if(is_cur_node( (rimeaddr_t*) data ))
  {
	  #if DEBUG > 0
		printf("Got message from current node.\n");
	  #endif
	  
	  // Blink status LED to indicate we got a node message
	  leds_on( LEDS_RED );
    
	  etimer_set(&et, CLOCK_SECOND / 8 );
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    
      leds_off( LEDS_RED );
	  
	  cur_node = cur_node + 1; 
  }
  
  PROCESS_END();
}

uint8_t is_cur_node( const rimeaddr_t* a )
{
  uint8_t retval = 0;
  
  if( a && (a->u8[0] == cur_node))
  {
    retval = 1;
  }
  
  return retval;
}
/*---------------------------------------------------------------------------*/

