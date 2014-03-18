/*
 * cycinc.c
 * 
 * Cyclic Incremental Algorithm Implementation 
 * 
 * The intermediate motes, and they will wait for a message 
 * from the upstream node, compute the local gradient and send it 
 * downstream.
 * 
 * The originator node, node 1, will compute the first gradient and wait
 * for the "go" signal from the master node, node 0.
 * 
 * Subfunctions are hard-coded. Function to optimize is global sum of
 * all subfunctions.
 * 
 */

/* 
 * Using fixed step size for now.
 * Actual step size is STEP/256, this is to keep all computations as 
 * integers
 */
#define STEP 2
#define START_VAL STEP
#define EPSILON 1       // Epsilon for stopping condition

#define NODE_ID 4       // One based
#define PREC_SHIFT 9

#define NODE_ADDR_0 NODE_ID
#define NODE_ADDR_1 0
#define NODE_ADDR_2 0

#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"

#include "cycinc.h"

/*
 * Global Variables
 */ 

//Variable storing previous cycle's local estimate for stop condition
static int32_t cur_data = 0;
static int16_t cur_cycle = 0;



/*
 * Local function declarations
 */
uint8_t is_from_upstream( opt_message_t* m );
uint8_t abs_diff(uint8_t a, uint8_t b);
int32_t abs_diff32(int32_t a, int32_t b);

/*
 * Sub-function
 * Computes the next iteration of the algorithm
 */
static int32_t grad_iterate(int32_t iterate)
{
  //return iterate;
  return ( iterate - ((STEP * ( (1 << (NODE_ID + 1))*iterate - (NODE_ID << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
}

/*
 * Communications handlers
 */
static struct broadcast_conn broadcast;

static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  static uint8_t stop = 0;
  
  static opt_message_t msg_recv;	
  static opt_message_t* msg = &msg_recv;
  packetbuf_copyto(msg);  
  
  opt_message_t out;
  /*
   * packetbuf_dataptr() should return a pointer to an opt_message_t,
   * but double-check to be sure.  Valid packets should start with
   * MKEY, and we're only interested in packets from our neighbors.
   */
  
  if(   NULL != msg 
     && !stop
     && is_from_upstream(msg) )
  {
    /*
     * Stopping condition
     */
    stop = ( ( abs_diff32(cur_data, msg->data) <= EPSILON ) && (cur_cycle > 1) );
 
    if(stop || msg->key == (MKEY + 1))
    {
      leds_on(LEDS_ALL);
  	  out.key = MKEY + 1;
      stop = 1;
    }
    else if(msg->key == MKEY)
    {
      leds_off(LEDS_ALL);
	  out.key = MKEY;
	}
      
    out.addr[0] = NODE_ADDR_0;
    out.addr[1] = NODE_ADDR_1;
    out.addr[2] = NODE_ADDR_2;
    out.iter = msg->iter + 1;
    out.data  = grad_iterate( msg->data );
    
    packetbuf_copyfrom( &out,sizeof(out) );
    broadcast_send(&broadcast);
    
    cur_data = msg->data;
	cur_cycle++;    
  }
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
	
PROCESS(main_process, "main");
AUTOSTART_PROCESSES(&main_process);
/*-------------------------------------------------------------------*/
PROCESS_THREAD(main_process, ev, data)
{
  static struct etimer et;
  
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();
  
  broadcast_open(&broadcast, COMM_CHANNEL, &broadcast_call);
  
#if NODE_ID == 1
  opt_message_t out;
  
  out.key = MKEY;
  out.addr[0] = NODE_ADDR_0;
  out.addr[1] = NODE_ADDR_1;
  out.addr[2] = NODE_ADDR_2;
  out.iter = 0;
  out.data  = START_VAL;
  
  packetbuf_copyfrom( &out,sizeof(out) );
  broadcast_send(&broadcast);
#endif
  
  while(1)
  {
    etimer_set(&et, CLOCK_SECOND * 4 );
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }
  
  PROCESS_END();
}
	
/*
 * Returns non-zero value if m originated from a neighbor node
 * Message is from a neighbor if addr[0] is NODE_ADDR_0 - 1
 */	
uint8_t is_from_upstream( opt_message_t* m )
{
  // Account for previous node.
  // If we are the first node, MAX_NODES is our upstream neighbor
  return (1 == (NODE_ADDR_0 - m->addr[0])) ||
         (NODE_ADDR_0 == 1 && m->addr[0] == MAX_NODES);
}

/*
 * Returns the absolute difference of two uint8_t's, which will
 * always be positive.
 */
uint8_t abs_diff(uint8_t a, uint8_t b)
{
  uint8_t ret;
  
  if( a > b )
    ret = a - b;
  else
    ret = b - a;
  
  return ret;  
}

/*
 * Returns the absolute difference of two int32_t's, which will
 * always be positive.
 */
int32_t abs_diff32(int32_t a, int32_t b)
{
  int32_t ret;
  
  if( a > b )
    ret = a - b;
  else
    ret = b - a;
  
  return ret;  
}

