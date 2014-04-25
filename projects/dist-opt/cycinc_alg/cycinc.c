/*
 * cycinc.c
 * 
 * Cyclic Incremental Algorithm Implementation 
 * 
 * The motes will wait for a message 
 * from the upstream node, compute the local gradient and send it 
 * downstream.
 * 
 * The originator node, node 1, will compute the first gradient
 * on startup and pass it along the chain.
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
#define STEP 2ll
#define PREC_SHIFT 9
#define START_VAL {30ll << PREC_SHIFT, 30ll << PREC_SHIFT, 10ll << PREC_SHIFT}
#define EPSILON 1       // Epsilon for stopping condition

#define CALIB_C 1     // Set to non-zero to calibrate on reset
#define MODEL_A (56000ll << PREC_SHIFT)
#define MODEL_B (3ll << PREC_SHIFT)
#define MODEL_C model_c
#define SPACING 30ll      // Centimeters of spacing

#define NUM_NODES 9
#define START_ID  10    // ID of first node in chain
#define START_NODE_0 10  // Address of node to start optimization algorithm
#define START_NODE_1 0
#define SNIFFER_NODE_0 25
#define SNIFFER_NODE_1 0
#define NODE_ID (rimeaddr_node_addr.u8[0])
#define MAX_ITER 500

#define DEBUG 0
#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 4

/*
 * Arrays to convert Node ID to row/column
 * Lower left node is at (0,0), and arrays are indexed 
 * with NODE_ID
 * 16 <- 15 <- 14
 *  |           |
 * 17 -> 18    13
 *     /        |
 * 10 -> 11 -> 12
 */
#define ID2ROW { 0, 0, 0, 1, 2, 2, 2, 1, 1 }
#define ID2COL { 0, 1, 2, 2, 2, 1, 0, 0, 1 }


#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"
#include "dev/light-sensor.h"
#include "dev/button-sensor.h"
#include "cycinc.h"

/*
 * Global Variables
 */ 

//Variable storing previous cycle's local estimate for stop condition
static int64_t cur_data[DATA_LEN] = {0};
static int16_t cur_cycle = 0;
static int64_t model_c = 88ll << PREC_SHIFT;

//Variables for bounding box conditions
static int64_t max_col = (90ll << PREC_SHIFT); 
static int64_t max_row = (90ll << PREC_SHIFT); 
static int64_t min_col = -1 * (30ll << PREC_SHIFT);
static int64_t min_row = -1 * (30ll << PREC_SHIFT);
static int64_t max_height = (30ll << PREC_SHIFT);
static int64_t min_height = (3ll << PREC_SHIFT);  

//Variables to store this node's immediate upstream neighbor and sniffer
static rimeaddr_t neighbor;
static rimeaddr_t sniffer;

/*
 * Local function declarations
 */
int64_t get_row();
int64_t get_col();

//uint8_t is_from_upstream( const rimeaddr_t* from );

uint8_t abs_diff(uint8_t a, uint8_t b);
int64_t abs_diff64(int64_t a, int64_t b);
int64_t norm2(int64_t* a, int64_t* b, int len);
int64_t g_model(int64_t* iterate);
int64_t f_model(int64_t* iterate);

/*
 * Processes
 */
PROCESS(main_process, "main");
PROCESS(rx_process, "rx_proc");
AUTOSTART_PROCESSES(&main_process);

/*
 * Sub-function
 * Computes the next iteration of the algorithm
 */
static void grad_iterate(int64_t* iterate, int64_t* result, int len)
{
  int i;
  
  //return iterate;
  //return ( iterate - ((STEP * ( (1 << (NODE_ID + 1))*iterate - (NODE_ID << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
  
  int64_t node_loc[3] = {get_col(), get_row(), 0};
  int64_t reading = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC) << PREC_SHIFT;
  
  for(i = 0; i < len; i++)
  {
    result[i] = iterate[i] - ((((STEP * 4ll * (MODEL_A * (reading - f_model(iterate)) / ((g_model(iterate) * g_model(iterate)) >> PREC_SHIFT))) >> PREC_SHIFT) * (iterate[i] - node_loc[i])) >> PREC_SHIFT);
  }
  
  /*
   * Bounding Box conditions to bring the iterate back if it strays too far 
   */
   //printf("result[0] = %"PRIi64" result[1] = %"PRIi64" result[2] = %"PRIi64"\n", result[0], result[1], result[2]);
   //printf("max_col = %"PRIi64" min_col = %"PRIi64" max_row = %"PRIi64" min_row = %"PRIi64" max_height = %"PRIi64" min_height = %"PRIi64"\n", max_col, min_col, max_row, min_row, max_height, min_height);
   if(result[0] > max_col)
   {
	   result[0] = max_col;
   }
   
   if(result[0] < min_col)
   {
	   result[0] = min_col;
   }

   if(result[1] > max_row)
   {
	   result[1] = max_row;
   }
   
   if(result[1] < min_row)
   {
     result[1] = min_row;
   }
   
   if(result[2] > max_height)
   {
	   result[2] = max_height;
   }
	  
   if(result[2] < min_height)
   {
	   result[2] = min_height;
   } 
  
}

/*
 * Communications handlers
 */
//static struct broadcast_conn broadcast;
static struct runicast_conn runicast;

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

static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
   //printf("runicast message received from %d.%d, seqno %d\n",
          //from->u8[0], from->u8[1], seqno);
  
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
//       printf("runicast message received from %d.%d, seqno %d (DUPLICATE)\n",
//              from->u8[0], from->u8[1], seqno);
      return;
    }
    /* Update existing history entry */
    e->seq = seqno;
  }
  
  process_start(&rx_process, (char*)from);
}

static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
   //printf("runicast message sent to %d.%d, retransmissions %d\n",
          //to->u8[0], to->u8[1], retransmissions);
}

static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  //printf("runicast message timed out when sending to %d.%d, retransmissions %d\n",
         //to->u8[0], to->u8[1], retransmissions);
}

//static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct runicast_callbacks runicast_callbacks = 
{ 
  recv_runicast,
  sent_runicast,
  timedout_runicast
};

/*-------------------------------------------------------------------*/
PROCESS_THREAD(main_process, ev, data)
{
  
  int i;
  model_c = 0;
  static int64_t s[3] = START_VAL;
  static opt_message_t out;
  static struct etimer et;
  
  if(NODE_ID  == (START_ID + NUM_NODES - 1))
    neighbor.u8[0] = START_ID;
  else 
    neighbor.u8[0] = NODE_ID + 1;
  
  neighbor.u8[1] = 0;
  
  sniffer.u8[0] = SNIFFER_NODE_0;
  sniffer.u8[1] = SNIFFER_NODE_1;
  
  PROCESS_EXITHANDLER(runicast_close(&runicast);)
  PROCESS_BEGIN();
  
  SENSORS_ACTIVATE(light_sensor);
  
  etimer_set(&et, CLOCK_SECOND*2);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  
  //broadcast_open(&broadcast, COMM_CHANNEL, &broadcast_call);
  runicast_open(&runicast, COMM_CHANNEL, &runicast_callbacks);

  
#if CALIB_C > 0
  /*
   * Code to calibrate light sensor at reset
   * Takes 50 readings and averages them, storing the value
   * in a global variable, model_c
   */
  
  for( i=0; i<50; i++ )
  {
    etimer_set(&et, CLOCK_SECOND/50);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    model_c += light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
  }
  
  model_c = (model_c / 50) << PREC_SHIFT;
  
  // Sniffer is expecting an opt_message_t
  out.key = NODE_ID;
  out.iter = 0;
  out.data[0] = model_c;
  out.data[1] = 0;
  out.data[2] = 0;
  
  while( runicast_is_transmitting(&runicast) )
  {
    etimer_set(&et, CLOCK_SECOND/32);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }
  
  packetbuf_copyfrom( &out,sizeof(out) );
  runicast_send(&runicast, &sniffer, MAX_RETRANSMISSIONS);
#endif
  
  if(rimeaddr_node_addr.u8[0] == START_NODE_0 &&
     rimeaddr_node_addr.u8[1] == START_NODE_1) 
  {
    SENSORS_ACTIVATE(button_sensor);
    
    // Don't start data collection until user button is pressed
    PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event 
                          && data == &button_sensor);
    
    etimer_set(&et, CLOCK_SECOND*2);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    
    out.key = MKEY;
    out.iter = 0;
    memcpy( out.data, s, DATA_LEN*sizeof(s[0]) );
    
    while( runicast_is_transmitting(&runicast) )
    {
      etimer_set(&et, CLOCK_SECOND/32);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
    
    packetbuf_copyfrom( &out,sizeof(out) );
    runicast_send(&runicast, &sniffer, MAX_RETRANSMISSIONS);
    
    while( runicast_is_transmitting(&runicast) )
    {
      etimer_set(&et, CLOCK_SECOND/32);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
   
    packetbuf_copyfrom( &out,sizeof(out) );
    runicast_send(&runicast, &neighbor, MAX_RETRANSMISSIONS);
  }
  
  while(1)
  {
    etimer_set(&et, CLOCK_SECOND * 4 );
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }
  
  SENSORS_DEACTIVATE(light_sensor);
  PROCESS_END();
}


/*
 * Started when a runicast packet is received.
 * 
 * 'data' is the 'from' pointer
 */
PROCESS_THREAD(rx_process, ev, data)
{
  PROCESS_BEGIN();
  /*
   * Process the packet 
   */
  
  static uint8_t stop = 0;
  
  static opt_message_t msg;
  packetbuf_copyto(&msg);  
  
  static opt_message_t out;
  
  
     //~ printf("%u %u", msg.iter, msg.key);
     //~ 
     //~ int i;
     //~ for( i=0; i<DATA_LEN; i++ )
     //~ {
       //~ printf(" %"PRIi64, msg.data[i]);
     //~ }
     //~ 
     //~ printf("\n");
       
  /*
   * packetbuf_dataptr() should return a pointer to an opt_message_t,
   * but double-check to be sure.  Valid packets should start with
   * MKEY, and we're only interested in packets from our neighbors.
   */
  if( !stop )
  {
    /*
     * Stopping condition
     */
    if (( norm2(cur_data, msg.data, DATA_LEN) <= EPSILON*EPSILON ) &&
      (cur_cycle > 1) )
    {
      stop++;
    }
    else
    {
      stop = 0;
    }
    
    if(stop == 10 || msg.key == (MKEY + 1) || out.iter >= MAX_ITER)
    {
      leds_on(LEDS_ALL);
      out.key = MKEY + 1;
      
      stop = 10;
    }
    else if(msg.key == MKEY)
    {
      leds_off(LEDS_ALL);
      out.key = MKEY;
      grad_iterate( msg.data, out.data, DATA_LEN );
    }
    
    out.iter = msg.iter + 1;
    
    while( runicast_is_transmitting(&runicast) )
    {
      static struct etimer et;
      
      etimer_set(&et, CLOCK_SECOND/32);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
    
    packetbuf_copyfrom( &out,sizeof(out) );
    runicast_send(&runicast, &sniffer, MAX_RETRANSMISSIONS);
    
    while( runicast_is_transmitting(&runicast) )
    {
      static struct etimer et;
      
      etimer_set(&et, CLOCK_SECOND/32);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
    
    packetbuf_copyfrom( &out,sizeof(out) );
    runicast_send(&runicast, &neighbor, MAX_RETRANSMISSIONS);
    
    // Save data for next time
    memcpy( cur_data, msg.data, DATA_LEN*sizeof(*cur_data) );
    cur_cycle++;  
    
    // Wait until we are done transmitting
    while( runicast_is_transmitting(&runicast) )
    {
      static struct etimer et;
      
      etimer_set(&et, CLOCK_SECOND/32);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    }
  }
  //   else
  //   {
  //     printf("Not from neighbor or stopping\n");
  //   }
  
  PROCESS_END();
}

/*
 * Returns non-zero value if m originated from a neighbor node
 * Message is from a neighbor if addr[0] is NODE_ADDR_0 - 1
 */	

uint8_t is_from_upstream( const rimeaddr_t* from )
{
  // Account for previous node.
  // If we are the first node, MAX_NODES is our upstream neighbor
  return(  ((NODE_ID) - from->u8[0]) == 1 
        || ( (NODE_ID==START_ID) 
           && (from->u8[0]==START_ID + NUM_NODES-1) ) );
}

/*
 * Returns row of node * spacing in cm
 */
int64_t get_row()
{
  int64_t r[] = ID2ROW;
  return ((r[ NODE_ID - START_ID ]) * SPACING) << PREC_SHIFT;
}

/*
 * Returns column of node * spacing in cm
 */
int64_t get_col()
{
  int64_t c[] = ID2COL;
  return ((c[ NODE_ID - START_ID ]) * SPACING) << PREC_SHIFT;
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
 * Returns the absolute difference of two int64_t's, which will
 * always be positive.
 */
int64_t abs_diff64(int64_t a, int64_t b)
{
  int64_t ret;
  
  if( a > b )
    ret = a - b;
  else
    ret = b - a;
  
  return ret;  
}

/*
 * Computes the denominator of model
 */
int64_t g_model(int64_t* iterate)
{
  int64_t abh[3];
  
  abh[0] = get_col();
  abh[1] = get_row();
  abh[2] = 0;
  return ((norm2( iterate, abh, DATA_LEN )) >> PREC_SHIFT) + MODEL_B;
  
  //return ((get_col() - iterate[0])*(get_col() - iterate[0]) + (get_row() - iterate[1])*(get_row() - iterate[1]) + (iterate[2])*(iterate[2])) >> PREC_SHIFT + MODEL_B;
}

/*
 * Computes the observation model function
 */
int64_t f_model(int64_t* iterate)
{
  return (MODEL_A << PREC_SHIFT)/g_model(iterate) + MODEL_C;
}

/*
 * Returns the squared norm of the vectors in a and b.  a and b
 * are assumed to be "shifted" by PREC_SHIFT.
 * Does no bounds checking.
 */
int64_t norm2(int64_t* a, int64_t* b, int len)
{
  int i;
  int64_t retval = 0;
  
  if( a != NULL && b != NULL )
  {
    for( i=0; i<len; i++ )
    {
      retval += (a[i] - b[i])*(a[i] - b[i]);
    }
  }
  
  return retval;
}
