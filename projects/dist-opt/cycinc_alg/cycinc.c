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
#define STEP 2
#define PREC_SHIFT 9
#define START_VAL {30 << PREC_SHIFT, 30 << PREC_SHIFT, 5 << PREC_SHIFT}
#define EPSILON 1       // Epsilon for stopping condition

#define MODEL_A (56000 << PREC_SHIFT)
#define MODEL_B (3 << PREC_SHIFT)
#define MODEL_C (72 << PREC_SHIFT)
#define SPACING 30      // Centimeters of spacing

#define NUM_NODES 9
#define START_ID  10    // ID of first node in chain
#define START_NODE_0 10  // Address of node to start optimization algorithm
#define START_NODE_1 0
#define NODE_ID (rimeaddr_node_addr.u8[0] - START_ID + 1)
#define MAX_ITER 500

/*
 * Arrays to convert Node ID to row/column
 * Lower left node is at (0,0), and arrays are indexed 
 * with NODE_ID
 */
#define ID2ROW { 0, 0, 0, 1, 2, 2, 2, 1, 1 }
#define ID2COL { 0, 1, 2, 2, 2, 1, 0, 0, 1 }
#define MAX_COL 2
#define MAX_ROW 2
#define MIN_COL 0
#define MIN_ROW 0

#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"
#include "dev/light-sensor.h"
#include "cycinc.h"

/*
 * Global Variables
 */ 

//Variable storing previous cycle's local estimate for stop condition
static int64_t cur_data[DATA_LEN] = {0};
static int16_t cur_cycle = 0;



/*
 * Local function declarations
 */
int64_t get_row();
int64_t get_col();

uint8_t is_from_upstream( const rimeaddr_t* from );
uint8_t abs_diff(uint8_t a, uint8_t b);
int64_t abs_diff64(int64_t a, int64_t b);
int64_t norm2(int64_t* a, int64_t* b, int len);
int64_t g_model(int64_t* iterate);
int64_t f_model(int64_t* iterate);

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
    result[i] = iterate[i] - ((((STEP * 4 * (MODEL_A * (reading - f_model(iterate)) / ((g_model(iterate) * g_model(iterate)) >> PREC_SHIFT))) >> PREC_SHIFT) * (iterate[i] - node_loc[i])) >> PREC_SHIFT);
  }
  
  /*
   * Bounding Box conditions to bring the iterate back if it strays too far 
   */
   if(result[1] > (((MAX_COL + 1) * SPACING) << PREC_SHIFT))
   {
	   result[1] = (((MAX_COL + 1) * SPACING) << PREC_SHIFT);
   }
   
   if(result[1] < (((MIN_COL - 1) * SPACING) << PREC_SHIFT))
   {
	   result[1] = (((MIN_COL - 1) * SPACING) << PREC_SHIFT);
   }

   if(result[2] > (((MAX_ROW + 1) * SPACING) << PREC_SHIFT))
   {
	   result[2] = (((MAX_ROW + 1) * SPACING) << PREC_SHIFT);
   }
   
   if(result[2] < (((ROW_COL - 1) * SPACING) << PREC_SHIFT))
   {
	   result[2] = (((ROW_COL - 1) * SPACING) << PREC_SHIFT);
   }
   
   if(result[3] > ((30 * SPACING) << PREC_SHIFT))
   {
	   result[3] = ((30 * SPACING) << PREC_SHIFT);
   }
	  
   if(result[3] < ((3 * SPACING) << PREC_SHIFT))
   {
	   result[3] = ((3 * SPACING) << PREC_SHIFT);
   } 
  
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
    && is_from_upstream(from) )
  {
    /*
     * Stopping condition
     */
    if (( norm2(cur_data, msg->data, DATA_LEN) <= EPSILON*EPSILON ) &&
        (cur_cycle > 1) )
    {
      stop++;
    }
    else
    {
      stop = 0;
    }
    
    if(stop == 10 || msg->key == (MKEY + 1) || out.iter >= MAX_ITER)
    {
      leds_on(LEDS_ALL);
      out.key = MKEY + 1;
      
      stop = 10;
    }
    else if(msg->key == MKEY)
    {
      leds_off(LEDS_ALL);
      out.key = MKEY;
      grad_iterate( msg->data, out.data, DATA_LEN );
    }
    
    out.iter = msg->iter + 1;
    
    packetbuf_copyfrom( &out,sizeof(out) );
    broadcast_send(&broadcast);
    
    memcpy( cur_data, msg->data, DATA_LEN*sizeof(*cur_data) );
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
  
  SENSORS_ACTIVATE(light_sensor);
  
  etimer_set(&et, CLOCK_SECOND*2);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  
  broadcast_open(&broadcast, COMM_CHANNEL, &broadcast_call);
  
  if(rimeaddr_node_addr.u8[0] == START_NODE_0 &&
    rimeaddr_node_addr.u8[1] == START_NODE_1) 
  {
	etimer_set(&et, CLOCK_SECOND*2);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));  
    
    int64_t s[3] = START_VAL;
    opt_message_t out;
    
    out.key = MKEY;
    out.iter = 0;
    memcpy( out.data, s, DATA_LEN );
    
    packetbuf_copyfrom( &out,sizeof(out) );
    broadcast_send(&broadcast);
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
  int r[] = ID2ROW;
  return ((r[ NODE_ID - START_ID ]) * SPACING) << PREC_SHIFT;
}

/*
 * Returns column of node * spacing in cm
 */
int64_t get_col()
{
  int c[] = ID2COL;
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
