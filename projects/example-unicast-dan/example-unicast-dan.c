#include "contiki.h"
#include "net/rime.h"

#include "dev/button-sensor.h"

#include "dev/leds.h"

#include <stdio.h>

/* 
 * Define the node address u8[0].u8[1]
 * For this example, program the receiver to have node address 1.0 and
 * the transmitter to have address 1.1
 */
//#define TX

#define RX_ADDR_0 1
#define RX_ADDR_1 0

#ifdef TX
  #define MY_ADDR_0 1
  #define MY_ADDR_1 1
#else
  #define MY_ADDR_0 RX_ADDR_0
  #define MY_ADDR_1 RX_ADDR_1
#endif /* TX */

#define MY_DEBUG
/*---------------------------------------------------------------------------*/
PROCESS(example_unicast_process, "Example unicast");
AUTOSTART_PROCESSES(&example_unicast_process);
/*---------------------------------------------------------------------------*/
static void
recv_uc(struct unicast_conn *c, const rimeaddr_t *from)
{
  printf("unicast message received from %d.%d\n",
         from->u8[0], from->u8[1]);
}
static void
send_uc(struct unicast_conn *c, int status, int num_tx)
{
#ifdef MY_DEBUG
  printf("Sending message. status: %d num_tx: %d\n", status, num_tx);
#endif /* MY_DEBUG */
  return;
}
static const struct unicast_callbacks unicast_callbacks = {recv_uc, send_uc};
static struct unicast_conn uc;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&uc);)
  
  PROCESS_BEGIN();
  
  unicast_open(&uc, 146, &unicast_callbacks);
  
  /* Set node address */
  rimeaddr_t my_addr;
  my_addr.u8[0] = MY_ADDR_0;
  my_addr.u8[1] = MY_ADDR_1;
  
  rimeaddr_set_node_addr( &my_addr );
  
#ifdef MY_DEBUG
  // Ensure that we set the current node address correctly.
  if( rimeaddr_cmp( &my_addr, &rimeaddr_node_addr ) )
    printf( "Addresses %d.%d and %d.%d are identical.", 
            rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], 
            my_addr.u8[0], my_addr.u8[1]);
  else
    printf( "Addresses %d.%d and %d.%d are NOT identical.", 
            rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], 
            my_addr.u8[0], my_addr.u8[1]);
#endif /* MY_DEBUG */
    
  
  while(1) {
    static struct etimer et;
    rimeaddr_t addr;
    static unsigned char counter = 0;
    
    counter++;
    
#ifdef TX
    static unsigned char i;
    static unsigned char j;
    for( i = 0; i <= 100; i++ )
    {
      for( j = 0; j <= 100; j++ )
      {
        etimer_set(&et, CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
#ifdef MY_DEBUG
        printf("i=%d, j=%d\n",i,j);
#endif /* MY_DEBUG */
        //     packetbuf_copyfrom(&counter, 1);
        addr.u8[0] = RX_ADDR_0;
        addr.u8[1] = RX_ADDR_1;
        
        packetbuf_copyfrom("Hello", 6);
        if(!rimeaddr_cmp(&addr, &rimeaddr_node_addr))
        {
          printf("Sending message to %d.%d. Returned: %d\n", RX_ADDR_0, RX_ADDR_1,
                 unicast_send(&uc, &addr));
          //unicast_send(&uc, &addr);
        }
      }
    }
#else
  etimer_set(&et, CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  
#ifdef MY_DEBUG
  printf("I am alive at %d.%d\n", (int)(rimeaddr_node_addr.u8[0]), (int)(rimeaddr_node_addr.u8[1]));
#endif /* MY_DEBUG */
#endif /* TX */
    
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
