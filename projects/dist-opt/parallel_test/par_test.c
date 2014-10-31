/*
 * par_test.c
 * 
 * Parallel Distributed Optimization Algorithm Implementation 
 * 
 * The motes will wait for a clock signal from a master node to 
 * signal the start of each round.
 *
 * Motes will update their local estimate with a local gradient and transmit to all neighbors,
 * They will average their local estimates with that of their neighbors. 
 * 
 * Subfunctions are hard-coded. Function to optimize is global sum of
 * all subfunctions.
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

#include "par_test.h"

/* 
 * Using fixed step size for now.
 * Actual step size is STEP/2^PREC_SHIFT, this is to keep all computations as 
 * integers
 */
#define STEP 8ll
#define PREC_SHIFT 9
#define START_VAL { 0 }
#define EPSILON 4ll      // Epsilon for stopping condition actual epsilon is this value divided by 2^PREC_SHIFT
#define CAUCHY_NUM 5    // Number of history elements for Cauchy test

// Model constants. Observation model follows (A/(r^2 + B)) + C
// g_model is the denominator, f_model is the entire expression
#define CALIB_C 0     // Set to non-zero to calibrate on reset
#define MODEL_A (48000ll << PREC_SHIFT)
#define MODEL_B (48ll << PREC_SHIFT)
#define MODEL_C model_c
#define SPACING 30ll      // Centimeters of spacing

// Special Node Addresses and Topology Constants

#define MAX_ROWS 1      // Max row number in sensor grid (0 - MAX_ROWS)
#define MAX_COLS 1      // Max column number in sensor grid (0 - MAX_COLS)
#define START_ID  10    // ID of first node in chain
#define SNIFFER_NODE_0 25
#define SNIFFER_NODE_1 0
#define CLOCK_NODE_0 20
#define CLOCK_NODE_1 0
#define NODE_ID (rimeaddr_node_addr.u8[0])
#define NORM_ID (rimeaddr_node_addr.u8[0] - START_ID + 1)

#define MAX_ITER 1000      // Max iteration number, algorithm will terminate at this point regardless of epsilon
#define RWIN 16ll          // Number of readings to average light sensor reading over

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
#define ID2ROW { 0, 0, 1, 1 }
#define ID2COL { 0, 1, 0, 1 }
#define ID2NUM_NEIGHBORS { 2, 2, 2, 2}
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
static int64_t tot_data[DATA_LEN] = {0};
static int16_t num_neighbor_messages_recv = 0;
static int16_t cur_cycle = 0;
static int16_t clock_msg_id = 0; 
static uint8_t stop = 0;
//static int64_t model_c = 88ll << PREC_SHIFT;

//Variables for bounding box conditions
//static int64_t max_col = (90ll << PREC_SHIFT); 
//static int64_t max_row = (90ll << PREC_SHIFT); 
//static int64_t min_col = -1 * (30ll << PREC_SHIFT);
//static int64_t min_row = -1 * (30ll << PREC_SHIFT);
//static int64_t max_height = (30ll << PREC_SHIFT);
//static int64_t min_height = (3ll << PREC_SHIFT); 

// List of neighbors
// All nodes have 4 "neighbors", but if they don't actually have that many, the vector 
// is padded with its own address. Only "neighbors" that have a different address
// than it's own, will be sent messages
static rimeaddr_t neighbors[MAX_NBRS]; 

// Various addresses
static rimeaddr_t sniffer;
static rimeaddr_t clock;

/*
 * Local function declarations
 */

// Functions to get location information from id
int64_t get_row();
int64_t get_col();

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
PROCESS(clock_rx_process, "clock_rx_proc");
AUTOSTART_PROCESSES(&main_process);

static struct broadcast_conn broadcast; 
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from){}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct runicast_conn runicast_clock;
static struct runicast_conn runicast;
/*
 * Sub-function
 * Computes the next iteration of the algorithm
 */
static void grad_iterate(int64_t* iterate, int64_t* result)
{
//  return iterate;
  *result = ( *iterate - ((STEP * ( (1 << (NORM_ID + 1))*(*iterate) - (NORM_ID << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
}


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
  
  uint8_t new_clock_msg = 0;
   
  if(rimeaddr_cmp(from, &clock))
  {
    clock_message_t tmp_msg;
	packetbuf_copyto(&tmp_msg);
	
	if(tmp_msg.id > clock_msg_id)
	{
		#if DEBUG>0
		 printf("Clock Message ID: %i Local Msg ID: %i\n", tmp_msg.id, clock_msg_id);
		#endif
				
		new_clock_msg = 1;
		clock_msg_id = tmp_msg.id;
	}
  }
  
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
    if(e->seq == seqno && (!rimeaddr_cmp(from, &clock) || (rimeaddr_cmp(from, &clock) && !new_clock_msg) ) )
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
  
  if((from->u8[0]) == CLOCK_NODE_0)
  {
      process_start(&clock_rx_process, (char*)(from));
  }
  else if(is_neighbor(from))
  {
  	  process_start(&nbr_rx_process, (char*)(from));
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

void comms_close(struct runicast_conn *c, struct broadcast_conn *b)
{
  runicast_close(c);
  broadcast_close(b);
}

/*-------------------------------------------------------------------*/
PROCESS_THREAD(main_process, ev, data)
{
  PROCESS_EXITHANDLER(comms_close(&runicast, &broadcast);)
  PROCESS_BEGIN();
 
  static struct etimer et;
      
  sniffer.u8[0] = SNIFFER_NODE_0;
  sniffer.u8[1] = SNIFFER_NODE_1;
  
  clock.u8[0] = CLOCK_NODE_0;
  clock.u8[1] = CLOCK_NODE_1;
    
  // Get neighbor list
  gen_neighbor_list();
  
  #if DEBUG > 0
    int64_t x = (1 << PREC_SHIFT);
    int64_t res;
    
    grad_iterate(&x, &res);
	printf("Gradient Test: x = 1, iterate = %"PRIi64"\n", res);
  #endif	
  
  SENSORS_ACTIVATE(light_sensor);
  
  etimer_set(&et, CLOCK_SECOND*2);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  
  broadcast_open(&broadcast, SNIFFER_CHANNEL, &broadcast_call);
  runicast_open(&runicast_clock, CLOCK_CHANNEL, &runicast_callbacks);
  runicast_open(&runicast, COMM_CHANNEL, &runicast_callbacks);
  
#if CALIB_C > 0
  /*
   * Code to calibrate light sensor at reset
   * Takes 50 readings and averages them, storing the value
   * in a global variable, model_c
   */
  static int i;
  model_c = 0;
  
  for( i=0; i<50; i++ )
  {
    etimer_set(&et, CLOCK_SECOND/50);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    model_c += light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
    //printf("model_c = %"PRIi64"\n", model_c);
  }
  
  model_c = (model_c / 50) << PREC_SHIFT;
  
  #if DEBUG > 0
    printf("Calibration Constant C = %"PRIi64"\n", model_c);
  #endif
  
#endif

  // Keep red LEDs on to indicate node is still alive
  leds_on( LEDS_RED );
  
  while(1)
  {
    etimer_set(&et, CLOCK_SECOND * 2 );
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));    
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
  static int i;
  
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
   * MKEY
   */
  if( !stop && msg.key == MKEY)
  {
    leds_off( LEDS_BLUE );

    // Add data to our running total for the round and increment received
    // message count 
    num_neighbor_messages_recv = num_neighbor_messages_recv + 1;

    for(i=0; i<DATA_LEN; i++)
    {
		tot_data[i] = tot_data[i] + msg.data[i];
	}  
  }
  else if((stop || msg.key == MKEY + 1) )
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
    printf("Number of neighbor messages this round: %u\n", num_neighbor_messages_recv);
    
  #endif 
  
  PROCESS_END();
}

/*
 * Started when a clock packet is received.
 * 
 * 'data' is the 'from' pointer
 */
PROCESS_THREAD(clock_rx_process, ev, data)
{
  PROCESS_BEGIN();
  
  static clock_message_t msg;
  packetbuf_copyto(&msg);
  
  static opt_message_t out;
  static int i;
  static struct etimer et;
  
  // Only start the round if the message is a clock type 
  if(msg.key == CKEY && !stop && cur_cycle <= MAX_ITER)
  {		
	  // Blink status LED to indicate we got a clock message
	  leds_off( LEDS_RED );
    
	  etimer_set(&et, CLOCK_SECOND / 8 );
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    
      leds_on( LEDS_RED );
	  
	  #if DEBUG > 0
        printf("Got clock message, round starting.\n");
	  #endif
	  
	  // Correct cycle number if incorrect, i.e. we missed a clock message
	  if(msg.cycle != cur_cycle)
	  {
		  cur_cycle = msg.cycle;
	  }

	  //static int64_t reading = 0;
	  	  
	  // Update local estimate with local gradient information
	  //reading = (((int64_t)light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC)) << PREC_SHIFT) - MODEL_C;
	  grad_iterate( cur_data, out.data);
	  
	  for(i=0; i<DATA_LEN; i++)
      {
		cur_data[i] = (out.data)[i];
      }
	  
	  // Check stop condition and set stop variable and change output message key if necessary
	  if(cauchy_conv(out.data) || cur_cycle == MAX_ITER)
	  {
		stop = 1;
		out.key = MKEY + 1;
	  }
	  
	  out.iter = cur_cycle;
	  out.key = MKEY;
	  
	  // Transmit local estimate to each neighbor in the neighbor list
	  for(i=0; i<MAX_NBRS; i++)
	  {
		// Do not transmit to ourselves obviously
		if(rimeaddr_cmp(&(neighbors[i]), &rimeaddr_node_addr))
		{
			continue;
		}
					
		#if DEBUG > 0
          printf("Transmitting to neighbor %d at %d.\n", i, (&(neighbors[i]))->u8[0]);
	    #endif
	    
        packetbuf_copyfrom( &out,sizeof(out) );	    
		runicast_send(&runicast, &(neighbors[i]), MAX_RETRANSMISSIONS);
	      
	    // Wait until we are done transmitting
	    while( runicast_is_transmitting(&runicast) )
	    {
		  etimer_set(&et, CLOCK_SECOND/32);
	      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	    }	    
	  }
	  
	  #if DEBUG > 0
        printf("Transmitting to sniffer.\n");
	  #endif
	  
	  // Unreliable broadcast of local estimate to the sniffer node
      packetbuf_copyfrom( &out,sizeof(out) );
	  broadcast_send(&broadcast);
	  
	  // Wait until we are done transmitting
	  etimer_set(&et, CLOCK_SECOND);
	  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	  
	  #if DEBUG > 0
        printf("Transmitting to clock.\n");
	  #endif
	  
	  packetbuf_copyfrom( &out,sizeof(out) );
	  runicast_send(&runicast, &clock, MAX_RETRANSMISSIONS);	  
	  
	  // Wait until we are done transmitting
	  while( runicast_is_transmitting(&runicast) )
	  {
	    etimer_set(&et, CLOCK_SECOND/32);
	    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	  }      	  	  
  }
  else if(msg.key == AKEY && !stop && cur_cycle <= MAX_ITER)
  {
      #if DEBUG > 0
        printf("Got clock message, averaging and finishing the round.\n");
	  #endif
      
      // Average local estimate with that of neighbors from the previous round, and reset aggregate data to zero
	  for(i=0; i<DATA_LEN; i++)
	  {
	    tot_data[i] = tot_data[i] + cur_data[i];
	  }
	  
	  for(i=0; i<DATA_LEN; i++)
	  {
	    cur_data[i] = tot_data[i]/(num_neighbor_messages_recv + 1);
	    tot_data[i] = 0;
	  }
	  
	  #if DEBUG > 0
	    printf("Averaged data given %u messages: ", num_neighbor_messages_recv);
	    
	    for( i=0; i<DATA_LEN; i++ )
        {
          printf(" %"PRIi64, cur_data[i]);
        }
        
        printf("\n");
      #endif
		  
	  // Reset number of received neighbor messages for the round and increment round counter
	  num_neighbor_messages_recv = 0;
	  cur_cycle = cur_cycle + 1;
	  
	  // Correct cycle number if incorrect, i.e. we missed a clock message
	  if(msg.cycle != cur_cycle)
	  {
		  cur_cycle = msg.cycle;
	  }
  }  
	  
  else if((msg.key == CKEY || msg.key == AKEY) && stop)
  {
      #if DEBUG > 0
          printf("Got clock message.\n");
	  #endif
    
      out.iter = cur_cycle;
      out.key = MKEY + 1;
    
	  leds_on( LEDS_BLUE );
	
	  // Blink status LED to indicate we got a clock message
	  leds_off( LEDS_RED );
    
	  etimer_set(&et, CLOCK_SECOND / 8 );
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    
      leds_on( LEDS_RED );
    
      // Unreliable broadcast of local estimate to the sniffer node
      for(i=0; i<DATA_LEN; i++)
	  {
		  (out.data)[i] = cur_data[i];
      }
    
      packetbuf_copyfrom( &out,sizeof(out) );
	  broadcast_send(&broadcast);
  } 		  
 
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

