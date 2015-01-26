/*
 * rp_bcast_test_ver2.c
 * 
 */
 
#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"
#include "dev/light-sensor.h"
#include "dev/button-sensor.h"
#include "lib/memb.h"

#include "rp_bcast.h"

/* 
 * Using fixed step size for now.
 * Actual step size is STEP/2^PREC_SHIFT, this is to keep all computations as 
 * integers
 */
#define TICK_PERIOD CLOCK_SECOND*4
#define STEP 32ll
#define PREC_SHIFT 9
#define START_VAL {30ll << PREC_SHIFT, 30ll << PREC_SHIFT, 10ll << PREC_SHIFT}
#define EPSILON 16ll      // Epsilon for stopping condition actual epsilon is this value divided by 2^PREC_SHIFT
#define CAUCHY_NUM 5    // Number of history elements for Cauchy test

// Model constants. Observation model follows (A/(r^2 + B)) + C
// g_model is the denominator, f_model is the entire expression
#define CALIB_C 1     // Set to non-zero to calibrate on reset
#define MODEL_A (47880ll << PREC_SHIFT)
#define MODEL_B (76ll << PREC_SHIFT)
#define MODEL_C model_c
#define SPACING 30ll      // Centimeters of spacing

// Special Node Addresses and Topology Constants

#define MAX_ROWS 2      // Max row number in sensor grid (0 to MAX_ROWS)
#define MAX_COLS 2      // Max column number in sensor grid (0 to MAX_COLS)
#define START_ID  10    // ID of first node in chain
#define SNIFFER_NODE_0 25
#define SNIFFER_NODE_1 0
#define NODE_ID (rimeaddr_node_addr.u8[0])
#define NORM_ID (rimeaddr_node_addr.u8[0] - START_ID + 1)

#define MAX_ITER 10000      // Max iteration number, algorithm will terminate at this point regardless of epsilon

//Debug printouts
#define DEBUG 1


/*
 * Arrays to convert Node ID to row/column
 * Top left node is at (0,0), and arrays are indexed 
 * with NODE_ID. Full grid topology, not single cycle.
 * All comm links are bi-directional, ordering is row-major
 *
 * 10 - 11 - 12
 *  |    |    |
 * 13 - 14 - 15
 *  |    |    |
 * 16 - 17 - 18
 */
#define ID2ROW { 0, 0, 0, 1, 1, 1, 2, 2, 2 }
#define ID2COL { 0, 1, 2, 0, 1, 2, 0, 1, 2 }
#define ID2NUM_NEIGHBORS { 2, 3, 2, 3, 4, 3, 2, 3, 2}
#define MAX_NBRS 4      // Max number of neighbors

/*
 * Global Variables
 */ 

/* Variables storing aggregate local estimates from neighbors,
 * current local estimate, data after gradient update,
 * number of received neighbor messages this round,
 * current iteration number for max iteration stopping,
 * nominal model_c in case calibration is disabled, and stop condition
 */
static int64_t cur_data[DATA_LEN] = START_VAL;
static int16_t cur_cycle = 0;
static uint8_t stop = 0;
static int64_t model_c = 85ll << PREC_SHIFT;

//Variables for bounding box conditions
static int64_t max_col = (90ll << PREC_SHIFT); 
static int64_t max_row = (90ll << PREC_SHIFT); 
static int64_t min_col = -1 * (30ll << PREC_SHIFT);
static int64_t min_row = -1 * (30ll << PREC_SHIFT);
static int64_t max_height = (30ll << PREC_SHIFT);
static int64_t min_height = (3ll << PREC_SHIFT); 

// List of neighbors
// All nodes have 4 "neighbors", but if they don't actually have that many, the vector 
// is padded with its own address. Only "neighbors" that have a different address
// than it's own, will be sent messages
static rimeaddr_t neighbors[MAX_NBRS];
static int64_t neighbor_vals[MAX_NBRS][DATA_LEN] = {{0}};
static uint8_t neighbor_msgs[MAX_NBRS] = {0};

/*
 * Local function declarations
 */

// Functions to get location information from id
int64_t get_row();
int64_t get_col();
uint8_t get_num_nbrs();

void rimeaddr2rc( rimeaddr_t a, unsigned int *row, unsigned int *col );
void rc2rimeaddr( rimeaddr_t* a , unsigned int row, unsigned int col );

uint8_t is_neighbor( const rimeaddr_t* a );
void gen_neighbor_list();
void init_node_addr();


// Functions that assist in gradient computation/ convergence criterion check
uint8_t abs_diff(uint8_t a, uint8_t b);
int64_t abs_diff64(int64_t a, int64_t b);
int64_t norm2(int64_t* a, int64_t* b, int len);
uint8_t cauchy_conv( int64_t* new );
int64_t g_model(int64_t* iterate);
int64_t f_model(int64_t* iterate);

/*
 * Communications handlers
 */
 
 /*
 * Processes
 */
PROCESS(main_process, "main");
PROCESS(nbr_rx_process, "nbr_rx_proc");
AUTOSTART_PROCESSES(&main_process);

static struct broadcast_conn broadcast_node; 
static void broadcast_recv_node(struct broadcast_conn *c, const rimeaddr_t *from)
{
  if(is_neighbor(from))
  {
    process_start(&nbr_rx_process, (char*)(from));
  }	
}
static const struct broadcast_callbacks broadcast_call_node = {broadcast_recv_node};

static struct broadcast_conn broadcast_sniffer;
static void broadcast_recv_sniffer(struct broadcast_conn *c, const rimeaddr_t *from){}
static const struct broadcast_callbacks broadcast_call_sniffer = {broadcast_recv_sniffer};

/*
 * Sub-function
 * Computes the next iteration of the algorithm
 */
static void grad_iterate(int64_t* iterate, int64_t* result, int len, int64_t reading)
{
  int i;
  int64_t node_loc[3] = {get_col(), get_row(), 0};
  
  for(i = 0; i < len; i++)
  {
    int64_t f = f_model(iterate);
    int64_t g = g_model(iterate);
    int64_t gsq = (g*g) >> PREC_SHIFT;
    
    /*
     * ( MODEL_A * (reading - f) * (iterate[i] - node_loc[i]) ) needs at 
     * most 58 bits, and after the division, is at least 4550.
     */
    result[i] = iterate[i] - ( (4ll * STEP * ( ((MODEL_A * (reading - f) * (iterate[i] - node_loc[i])) / gsq) >> PREC_SHIFT)) >> PREC_SHIFT);
  }
  
  /*
   * Bounding Box conditions to bring the iterate back if it strays too far 
   */

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

void comms_close(struct broadcast_conn *c, struct broadcast_conn *b)
{
  broadcast_close(c);
  broadcast_close(b);
}

/*-------------------------------------------------------------------*/
PROCESS_THREAD(main_process, ev, data)
{
  PROCESS_EXITHANDLER(comms_close(&broadcast_sniffer, &broadcast_node);)
  PROCESS_BEGIN();
  
  static struct etimer et;
  static opt_message_t tick_msg;
  static int i, j, min_msgs, num_nbrs;
  static int64_t reading = 0;
    
  // Get neighbor list
  gen_neighbor_list();
  num_nbrs = get_num_nbrs();
  
  SENSORS_ACTIVATE(light_sensor);
  
  etimer_set(&et, CLOCK_SECOND*2);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));  
  
  #if CALIB_C > 0
    /*
     * Code to calibrate light sensor at reset
     * Takes 50 readings and averages them, storing the value
     * in a global variable, model_c
     */
    model_c = 0;
  
    for( i=0; i<50; i++ )
    {
      etimer_set(&et, CLOCK_SECOND/50);
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      model_c += light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
    }
  
    model_c = (model_c / 50) << PREC_SHIFT;
  
    #if DEBUG > 0
      printf("Calibration Constant C = %"PRIi64"\n", model_c);
    #endif
  
  #endif
  
  // Don't start algorithm until user button is pressed
  SENSORS_ACTIVATE(button_sensor);    
  
  PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event 
                          && data == &button_sensor);
  
  etimer_set(&et, CLOCK_SECOND*5);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  
  broadcast_open(&broadcast_sniffer, SNIFFER_CHANNEL, &broadcast_call_sniffer);
  broadcast_open(&broadcast_node, COMM_CHANNEL, &broadcast_call_node);

  while(1)
  {
    etimer_set(&et, TICK_PERIOD);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    
    // Blink Red LEDs to indicate we have a clock tick
    leds_on( LEDS_RED );
    
    etimer_set(&et, CLOCK_SECOND / 8 );
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
   
    leds_off( LEDS_RED );
    
    #if DEBUG > 0
      printf("Clock Tick\n");
    #endif    
    
    tick_msg.key = TKEY + stop;	
	tick_msg.iter = cur_cycle;
	tick_msg.node = NODE_ID;
	
	for( i=0; i<DATA_LEN; i++ )
    {
      tick_msg.data[i]  = cur_data[i];
    }
    

    //Unreliably broadcast iterate to all neighbors in sequence
    #if DEBUG > 0
      printf("Broadcasting to neighbors.\n");
	#endif
	
    packetbuf_copyfrom( &tick_msg,sizeof(tick_msg) );	    
	broadcast_send(&broadcast_node);

	// Unreliable broadcast of local estimate to the sniffer node
    #if DEBUG > 0
      printf("Broadcasting to sniffer.\n");
	#endif
    
	packetbuf_copyfrom( &tick_msg,sizeof(tick_msg) );
	broadcast_send(&broadcast_sniffer);
	
	min_msgs = 1000;
	for(i=0; i<MAX_NBRS; i++)
	{
		if(!rimeaddr_cmp(&(neighbors[i]), &rimeaddr_node_addr) && neighbor_msgs[i] < min_msgs)
		{
			#if DEBUG > 0
			  printf("Got %i msgs from node %i\n", neighbor_msgs[i], (neighbors[i]).u8[0]);
			#endif
			
			min_msgs = neighbor_msgs[i];
		}
	}	
	
	// If we are not stopping and we got at least one message from each neighbor, update with averaging and gradient
	#if DEBUG > 0
	  printf("Current min_msgs: %i\n", min_msgs);
	#endif 
	      
    if(!stop && min_msgs > 0)
    {
      for(i=0; i<MAX_NBRS; i++)
      {
		  if(!rimeaddr_cmp(&(neighbors[i]), &rimeaddr_node_addr))
		  {
			  for(j=0; j<DATA_LEN; j++)
			  {
				  // When aggregating, average over number of neighbor updates for each neighbor's value
				  cur_data[j] = cur_data[j] + (neighbor_vals[i][j]/neighbor_msgs[i]);
			  }
		  }
	  }
	  
	  // Average over all neighbors and current node
	  for(j=0;j<DATA_LEN;j++)
	  {
		  cur_data[j] = cur_data[j]/(1 + num_nbrs);
	  }
      
      // Update with gradient and re-copy to current data
      reading = (((int64_t)light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC)) << PREC_SHIFT) - MODEL_C;
      grad_iterate( cur_data, tick_msg.data, DATA_LEN, reading);

      for(j=0;j<DATA_LEN;j++)
	  {
		  cur_data[j] = (tick_msg.data)[j];
	  }
      
      // Check stop condition and set stop variable
	  if(cauchy_conv(cur_data) || cur_cycle == MAX_ITER)
	  {
	    stop = 1;
	  }
      
      // Reset neighbor counts
      for(i=0;i<MAX_NBRS;i++)
      {
	    if(!rimeaddr_cmp(&(neighbors[i]), &rimeaddr_node_addr))
		{
		  neighbor_msgs[i] = 0;
		  for(j=0;j<DATA_LEN;j++)
		  {
		    neighbor_vals[i][j] = 0;
		  }
		}
	  }                
    }
    
    else if(stop)
	{
	  leds_on( LEDS_BLUE );
	}
	
	cur_cycle = cur_cycle + 1;      
  }
  
  //SENSORS_DEACTIVATE(light_sensor);
  PROCESS_END();
}

/*
 * Started when a neighbor runicast packet is received.
 * 
 * 'data' is the pointer to the id of the last sender
 */
PROCESS_THREAD(nbr_rx_process, ev, data)
{
  PROCESS_BEGIN();
  
  static struct etimer et;
  static opt_message_t msg;
  static int i, j;
  
  #if DEBUG > 0
      printf("Got neighbor message.\n");
  #endif
  
  /*
   * Process the packet 
   */
  packetbuf_copyto(&msg);
  
  // Blink Green LEDs to indicate we got a neighbor message 
  leds_on( LEDS_GREEN );
    
  etimer_set(&et, CLOCK_SECOND / 8 );
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
   
  leds_off( LEDS_GREEN );
        
  /*
   * packetbuf_dataptr() should return a pointer to an opt_message_t,
   * but double-check to be sure.  Valid packets should start with
   * TKEY
   */
  if( !stop && msg.key == TKEY && cur_cycle < MAX_ITER)
  {
    #if DEBUG > 0
      printf("Neighbor sent clock tick\n");
    #endif
    
    leds_off( LEDS_BLUE );
    
    // Record message as being received and add it to relevant running totals
    for(i=0; i<MAX_NBRS; i++)
    {
		if(!rimeaddr_cmp(&(neighbors[i]), &rimeaddr_node_addr) && (msg.node == (neighbors[i]).u8[0]))
		{
			#if DEBUG > 0
			  printf("Updating aggregate neighbor counts.");
			#endif
			
			neighbor_msgs[i] = neighbor_msgs[i] + 1;
			
			for(j=0; j<DATA_LEN; j++)
			{
				neighbor_vals[i][j] = neighbor_vals[i][j] + msg.data[j];
			}
		}
	}	
  }
  
  else if(stop || (msg.key == TKEY + 1))
  {
	  stop = 1;
	  leds_on( LEDS_BLUE );
  }  
  
  #if DEBUG > 0
    printf("Data Received: \n");
    printf("%u %u", msg.iter, msg.key);

    for( i=0; i<DATA_LEN; i++ )
    {
      printf(" %"PRIi64, msg.data[i]);
    }
  
    printf("\n");
   
  #endif 
  
  PROCESS_END();
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

uint8_t get_num_nbrs()
{
	uint8_t n[] = ID2NUM_NEIGHBORS;
	return( n[NODE_ID - START_ID]);
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
  return (MODEL_A << PREC_SHIFT)/g_model(iterate);
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

/*
 * Calculates the rime address of the node at (row, col) and writes it
 * in a.  row and col are one-based (there is no row 0 or col 0).
 * 
 * Assumes nodes are in row major order (e.g., row 0 contains 
 * nodes 1,2,3,...
 */
void rc2rimeaddr( rimeaddr_t* a , unsigned int row, unsigned int col )
{
  if( a )
  {
    a->u8[0] = (START_ID) + (row)*(MAX_COLS + 1) + col;
    a->u8[1] = 0;
  }
}

/*
 * Calculates the row and column of the node with the rime address a
 * and writes it into row and col.
 */
void rimeaddr2rc( rimeaddr_t a, unsigned int *row, unsigned int *col )
{
  if( row && col )
  {
    *row = ((a.u8[0] - START_ID ) / (MAX_COLS + 1));
    *col = ((a.u8[0] - START_ID ) % (MAX_COLS + 1));
  }
}

/*
 * Returns non-zero if the Cauchy condition is met for the 
 * last CAUCHY_NUM elements.
 * 
 * 'new' is the newest element in the sequency, and 'eps' is the
 * threshold for the stopping condition.
 * 
 * Keeps an array of the last CAUCHY_NUM elements.  If all the
 * elements are within 'eps' of eachother, then the Cauchy 
 * condition is met.
 */
uint8_t cauchy_conv( int64_t* new )
{
  static int64_t seq[CAUCHY_NUM][DATA_LEN]; // Sequence
  static unsigned int count = 0;            // Number of elements
  int i, j;
  uint8_t retval = 0;
  
  if( new )
  {
    memcpy( seq[count%CAUCHY_NUM], new, DATA_LEN*sizeof(new[0]) );
    count++;
    
    if( count >= CAUCHY_NUM )
    {
      retval = 1;
      
      for( i=0; i<CAUCHY_NUM && retval; i++ )
      {
        for( j=CAUCHY_NUM-1; j>i && retval; j-- )
        {
          if( norm2( seq[i], seq[j], DATA_LEN ) > (EPSILON*EPSILON) )
          {
            retval=0;
          }
        }
      }
    }
  }
  else
  {
    count = 0;
  }
  
  return retval;
}

/*
 * Returns non-zero if a is in the neighbor list
 */
uint8_t is_neighbor( const rimeaddr_t* a )
{
  uint8_t i, retval = 0;
  
  if( a )
  {
    for( i=0; i<MAX_NBRS; i++ )
    {
      if(!rimeaddr_cmp(&(neighbors[i]), &rimeaddr_node_addr))
        {
	      retval = retval || rimeaddr_cmp(&(neighbors[i]), a);
		}
    }
  }
  
  return retval;
}

/*
 * Creates list of neighbors, storing it in a global variable
 * static rimeaddr_t neighbors[NUM_NBRS];
 * 
 * If neighbor in any direction does not exist, then its address
 * is given by this node's address.
 */
void gen_neighbor_list()
{
  rimeaddr_t a;
  unsigned int row, col;
  
  // Get our row and column
  rimeaddr2rc( rimeaddr_node_addr, &row, &col );
  
  // Get rime addresses of neighbor nodes
  
  // North neighbor, ensure row != 0
  if( row == 0 )
  {
    // Can't go North, use our address
    rimeaddr_copy( &(neighbors[0]), &rimeaddr_node_addr );
  }
  else
  {
    // Get address of node to North, copy to neighbors list
    rc2rimeaddr( &a, row-1, col );
    rimeaddr_copy( &(neighbors[0]), &a );
  }
    
  // East neighbor, ensure col != MAX_COLS
  if( col == MAX_COLS )
  {
    // Can't go East, use our address
    rimeaddr_copy( &(neighbors[1]), &rimeaddr_node_addr );
  }
  else
  {
    // Get address of node to East, copy to neighbors list
    rc2rimeaddr( &a, row, col+1 );
    rimeaddr_copy( &(neighbors[1]), &a );
  }
  
  // South neighbor, ensure row != MAX_ROWS
  if( row ==  MAX_ROWS )
  {
    // Can't go South, use our address
    rimeaddr_copy( &(neighbors[2]), &rimeaddr_node_addr );
  }
  else
  {
    // Get address of node to South, copy to neighbors list
    rc2rimeaddr( &a, row+1, col );
    rimeaddr_copy( &(neighbors[2]), &a );
  }
    
  // West neighbor, ensure col != 1
  if( col == 0 )
  {
    // Can't go West, use our address
    rimeaddr_copy( &(neighbors[3]), &rimeaddr_node_addr );
  }
  else
  {
    // Get address of node to West, copy to neighbors list
    rc2rimeaddr( &a, row, col-1 );
    rimeaddr_copy( &(neighbors[3]), &a );
  }
  
#if DEBUG > 0
  int i;
  
  for( i=0; i<MAX_NBRS; i++ )
  {
    printf("Neighbor %d at %d.%d\n", i, (neighbors[i]).u8[0], (neighbors[i]).u8[1]);
  }
  
  printf("Normalized ID is %i.\n", NORM_ID);
#endif
}

