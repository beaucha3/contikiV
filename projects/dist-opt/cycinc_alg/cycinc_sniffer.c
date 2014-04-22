#include "contiki.h"
#include "net/rime.h"

#define TX 0

#include <stdio.h>

//#include "shell.h"
#include "cycinc.h"
/*---------------------------------------------------------------------------*/
PROCESS(example_broadcast_process, "Broadcast example");
AUTOSTART_PROCESSES(&example_broadcast_process);
/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
   //printf("broadcast message received from %d.%d: '%s'\n",
          //from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

	static opt_message_t msg;
  int i;

	packetbuf_copyto(&msg);
  
  printf("%u %u", msg.iter, msg.key);
  
  for( i=0; i<DATA_LEN; i++ )
  {
    printf(" %lli", msg.data[i]);
  }
  
  printf("\n");
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_broadcast_process, ev, data)
{
  static struct etimer et;
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();

  broadcast_open(&broadcast, COMM_CHANNEL, &broadcast_call);

  while(1) 
  {
    /* Delay 2-4 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 );

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

