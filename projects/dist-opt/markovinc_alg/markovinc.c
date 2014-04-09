/*
 * markovinc.c
 * 
 * (Neeraj, please update this)
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

#define DEBUG 3

/* 
 * Using fixed step size for now.
 * Actual step size is STEP/256, this is to keep all computations as 
 * integers
 */
#define STEP 2
#define START_VAL STEP
#define EPSILON 1       // Epsilon for stopping condition

#define MAX_ROWS 3      // Number of rows in sensor grid
#define MAX_COLS 3      // Number of columns in sensor grid
#define NUM_NBRS 5      // Number of neighbors, including self
#define START_ID 10     // ID of top left node in grid

#define NODE_ID (rimeaddr_node_addr.u8[0] - START_ID + 1)

#define START_NODE_0 10  // Address of node to start optimization algorithm
#define START_NODE_1 0

#define PREC_SHIFT 9

#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 4

#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "random.h"

#include "markovinc.h"

/*
 * Global Variables
 */ 

// Variable storing previous cycle's local estimate for stop condition
static int32_t cur_data[DATA_LEN] = {0};
static int16_t cur_cycle = 0;

// List of neighbors
static rimeaddr_t neighbors[NUM_NBRS];


/*
 * Local function declarations
 */
void rimeaddr2rc( rimeaddr_t a, unsigned int *row, unsigned int *col );
void rc2rimeaddr( rimeaddr_t* a , unsigned int row, unsigned int col );

static void message_recv(const rimeaddr_t *from);

uint8_t send_to_neighbor();
void flood_network( const rimeaddr_t *from, opt_message_t *msg );
uint8_t is_neighbor( const rimeaddr_t* a );
void gen_neighbor_list();

uint8_t abs_diff(uint8_t a, uint8_t b);
int32_t abs_diff32(int32_t a, int32_t b);
int32_t norm2(int32_t* a, int32_t* b, int len);

/*
 * Sub-function
 * Computes the next iteration of the algorithm
 */
static int32_t grad_iterate1(int32_t iterate)
{
//  return iterate;
  return ( iterate - ((STEP * ( (1 << (NODE_ID + 1))*iterate - (NODE_ID << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
}

static int32_t grad_iterate2(int32_t iterate)
{
  //  return iterate;
  return ( iterate - ((STEP * ( (1 << ((NODE_ID) + 1))*iterate - (NODE_ID << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
}

/*
 * Communications handlers
 */

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
/*---------------------------------------------------------------------------*/
static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
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
  
  /*
   * Call function to do something with packet 
   */
  message_recv( from );
  
//   printf("runicast message received from %d.%d, seqno %d\n",
//          from->u8[0], from->u8[1], seqno);
}

static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
//   printf("runicast message sent to %d.%d, retransmissions %d\n",
//          to->u8[0], to->u8[1], retransmissions);
}

static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  printf("runicast message timed out when sending to %d.%d, retransmissions %d\n",
         to->u8[0], to->u8[1], retransmissions);
}

static const struct runicast_callbacks runicast_callbacks = 
{ 
  recv_runicast,
  sent_runicast,
  timedout_runicast
};

static struct runicast_conn runicast;
/*-----------------------------------------------------------------------*/

PROCESS(main_process, "main");
AUTOSTART_PROCESSES(&main_process);
/*-------------------------------------------------------------------*/
PROCESS_THREAD(main_process, ev, data)
{
  static struct etimer et;
  
  PROCESS_EXITHANDLER(runicast_close(&runicast);)
  
  PROCESS_BEGIN();
  
  runicast_open(&runicast, 144, &runicast_callbacks);
  
  /* Sender history */
  list_init(history_table);
  memb_init(&history_mem);
  
  gen_neighbor_list();
  
  // Seed random number generator with node's address
  random_init(rimeaddr_node_addr.u8[0] + rimeaddr_node_addr.u8[1]);
  
  if(rimeaddr_node_addr.u8[0] == START_NODE_0 &&
    rimeaddr_node_addr.u8[1] == START_NODE_1) 
  {
    int i;
    opt_message_t out;
    rimeaddr_t* to = &(neighbors[NUM_NBRS]);
    
    for( i=1; i<NUM_NBRS; i++ )
    {
      /*
       * If addresses are different, send there
       */
      if( !(rimeaddr_cmp(&(neighbors[i]), &rimeaddr_node_addr)) )
      {
        to = &(neighbors[i]);
        break;
      }
    }
    
    out.key = MKEY;
    out.iter = 0;
    
    for( i=0; i<DATA_LEN; i++ )
    {
      out.data[i]  = START_VAL;
    }
    
    packetbuf_copyfrom( &out,sizeof(out) );
    runicast_send(&runicast, to, MAX_RETRANSMISSIONS);
  }
  
  while(1)
  {
    etimer_set(&et, CLOCK_SECOND * 4 );
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }
  
  PROCESS_END();
}


static void message_recv(const rimeaddr_t *from)
{
  int i;
  static uint8_t stop = 0;
  
  static opt_message_t msg;
  packetbuf_copyto(&msg);  
  
  /*
   * We're only interested in packets from our neighbors, and only 
   * if we haven't stopped.
   */
  if( is_neighbor( from ) )
  {
    /*
     * Update stop condition - we need to find that the iterate we
     * just received is within EPSILON of our current iterate at this
     * node STOP_THRES times
     * 
     * Otherwise, reset the stop count
     */
    
    // Check if someone else has converged
    if( msg.key == (MKEY + 1) )
    {
      stop = STOP_THRES;
    }
    
    
    // norm2 returns norm shifted left by PREC_SHIFT
    if ( ( norm2(cur_data, msg.data, DATA_LEN) <= 
         (EPSILON*EPSILON) << PREC_SHIFT ) 
       &&(cur_cycle > 1)
       &&(stop < STOP_THRES) )
    {
      // We are within EPSILON, but no one else is, increment stop counter
      stop++;
    }
    else if( stop >= STOP_THRES )
    {
      // Someone has converged, set stop counter
      stop = STOP_THRES;
    }
    else
    {
      // No one has converged yet.
      stop = 0;
    }
    
    // stop = STOP_THRES if msg.key == MKEY+1
    if( stop >= STOP_THRES )
    {
      #if DEBUG > 0
      printf("Stop condition met.\n");
      #endif
      leds_on(LEDS_ALL);
      msg.key = MKEY + 1;
      // stop is set before loop
      //stop = 1;
      
      // We have converged!  Flood network with the good news!
      flood_network( from, &msg );
    }
    else if( msg.key == MKEY )
    {
      
      /*
       * Send the data to one of our neighbors
       * send_to_neighbor() returns non-zero if we sent to ourselves
       * If we sent to ourselves, try again.
       */
      do
      {
        
#if DEBUG > 2
        for( i=0; i<DATA_LEN; i++ )
        {
          printf("%ld ", cur_data[i]);
        }
        printf("\n");
        
        for( i=0; i<DATA_LEN; i++ )
        {
          printf("%ld ", (msg.data)[i]);
        }
        printf("\n");
#endif


#if DEBUG > 0
        // Print message we just received
        printf("Received from %d.%d:\n",(from->u8)[0],(from->u8)[1] );
        printf("%u, %u", msg.key, msg.iter);
        for( i=0; i<DATA_LEN; i++ )
        {
          printf(", %ld", msg.data[i]);
        }
        printf("\n");
#endif
        
        memcpy( cur_data, msg.data, DATA_LEN*sizeof(int32_t) );
        cur_cycle++;
        
        leds_off(LEDS_ALL);
        msg.key = MKEY;
        
        // This is really inelegant, but it should work
        msg.data[0]  = grad_iterate1( msg.data[0] );
        msg.data[1]  = grad_iterate2( msg.data[1] );
        
        
        msg.iter = msg.iter + 1;
        
        // Print message we are about to send
        printf("%u, %u", msg.key, msg.iter);
        for( i=0; i<DATA_LEN; i++ )
        {
          printf(", %ld", msg.data[i]);
        }
        printf("\n");
        
        packetbuf_copyfrom( &msg,sizeof(msg) );
      }
      while( send_to_neighbor() );
    }
  }
#if DEBUG > 0
  else( !stop )
  {
    printf("Message received from %d.%d - not neighbor\n",(from->u8)[0],(from->u8)[1] );
  }
#endif
}

/*
 * Generate a random number from 0 through 4 and send
 * the packet buffer to that node
 * 
 * Returns non-zero if sent to self, 0 if sent to external node
 */
uint8_t send_to_neighbor()
{
  uint8_t r, retval;
  
  r = random_rand() % NUM_NBRS;
  
#if DEBUG > 0
  printf("Sending to neighbor at %d.%d\n", (neighbors[r]).u8[0], (neighbors[r]).u8[1] );
#endif
  
  // Don't send to self
  if( !( retval = rimeaddr_cmp(&(neighbors[r]), &rimeaddr_node_addr)) )
  {
    runicast_send(&runicast, &(neighbors[r]), MAX_RETRANSMISSIONS);
  }
  
  return retval;
}

/*
 * Loops through all neighbors, sending a packet to all except 
 * this node and 'from'.
 */
void flood_network( const rimeaddr_t *from, opt_message_t *msg )
{
  int r;
  
  if( from && msg )
  {
    // neighbors[0] is this node, can skip
    for( r=1; r<NUM_NBRS; r++ )
    {
      // Send only if not this node and is not node we just received from
      if( (!rimeaddr_cmp(&(neighbors[r]), &rimeaddr_node_addr))
        &&(!rimeaddr_cmp(&(neighbors[r]), from)) )
      {
        packetbuf_copyfrom( msg, sizeof(opt_message_t) );
        runicast_send(&runicast, &(neighbors[r]), MAX_RETRANSMISSIONS);
      }
    }
  }
}

/*
 * Returns non-zero if a is in the neighbor list
 */
uint8_t is_neighbor( const rimeaddr_t* a )
{
  uint8_t i, retval = 0;
  
  if( a )
  {
    for( i=0; i<NUM_NBRS; i++ )
    {
      retval = retval || rimeaddr_cmp(&(neighbors[i]), a);
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
  
  // Define first "neighbor" to be this node
  rimeaddr_copy( &(neighbors[0]), &rimeaddr_node_addr );
  
  // Get rime addresses of neighbor nodes
  
  // North neighbor, ensure row != 1
  if( row == 1 )
  {
    // Can't go North, use our address
    rimeaddr_copy( &(neighbors[1]), &rimeaddr_node_addr );
  }
  else
  {
    // Get address of node to North, copy to neighbors list
    rc2rimeaddr( &a, row-1, col );
    rimeaddr_copy( &(neighbors[1]), &a );
  }
    
  // East neighbor, ensure col != MAX_COLS
  if( col == MAX_COLS )
  {
    // Can't go East, use our address
    rimeaddr_copy( &(neighbors[2]), &rimeaddr_node_addr );
  }
  else
  {
    // Get address of node to East, copy to neighbors list
    rc2rimeaddr( &a, row, col+1 );
    rimeaddr_copy( &(neighbors[2]), &a );
  }
  
  // South neighbor, ensure row != MAX_ROWS
  if( row ==  MAX_ROWS )
  {
    // Can't go South, use our address
    rimeaddr_copy( &(neighbors[3]), &rimeaddr_node_addr );
  }
  else
  {
    // Get address of node to South, copy to neighbors list
    rc2rimeaddr( &a, row+1, col );
    rimeaddr_copy( &(neighbors[3]), &a );
  }
    
  // West neighbor, ensure col != 1
  if( col == 1 )
  {
    // Can't go West, use our address
    rimeaddr_copy( &(neighbors[4]), &rimeaddr_node_addr );
  }
  else
  {
    // Get address of node to West, copy to neighbors list
    rc2rimeaddr( &a, row, col-1 );
    rimeaddr_copy( &(neighbors[4]), &a );
  }
  
#if DEBUG > 1
  int i;
  
  for( i=0; i<5; i++ )
  {
    printf("Neighbor %d at %d.%d\n", i, (neighbors[i]).u8[0], (neighbors[i]).u8[1]);
  }
#endif
}

/*
 * Calculates the rime address of the node at (row, col) and writes it
 * in a.  row and col are one-based (there is no row 0 or col 0).
 * 
 * Assumes nodes are in row major order (e.g., row 1 contains 
 * nodes 1,2,3,...
 */
void rc2rimeaddr( rimeaddr_t* a , unsigned int row, unsigned int col )
{
  if( a )
  {
    a->u8[0] = (START_ID - 1) + (row-1)*MAX_COLS + col;
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
    *row = ((a.u8[0] - START_ID ) / MAX_COLS) + 1;
    *col = ((a.u8[0] - START_ID ) % MAX_COLS) + 1;
  }
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

/*
 * Returns the squared norm of the vectors in a and b.  a and b
 * are assumed to be "shifted" by PREC_SHIFT.
 * Does no bounds checking.
 */
int32_t norm2(int32_t* a, int32_t* b, int len)
{
  int i;
  int32_t retval = 0;

#if DEBUG > 1
  for( i=0; i<len; i++ )
  {
    printf("%ld ", a[i]);
  }
  printf("\n");
  for( i=0; i<len; i++ )
  {
    printf("%ld ", b[i]);
  }
  printf("\n");
#endif
  
  if( a != NULL && b != NULL )
  {
    for( i=0; i<len; i++ )
    {
      retval += (a[i] - b[i])*(a[i] - b[i]);
    }
  }
  
#if DEBUG > 2
  printf("Norm2 returning %ld\n", retval);
#endif
  
  return retval;
}
