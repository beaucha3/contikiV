#include "contiki.h"
#include "net/rime.h"
#include <stdio.h>
#include "par_test_async.h"

#define TX 0
/*---------------------------------------------------------------------------*/
PROCESS(broadcast_receiver_process, "Broadcast Receiver");
AUTOSTART_PROCESSES(&broadcast_receiver_process);
/*---------------------------------------------------------------------------*/
static struct broadcast_conn broadcast;

static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  //printf("broadcast message received from %d.%d: '%s'\n",
          //from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

  static opt_message_t msg;
  int i;

  packetbuf_copyto(&msg);
  
  // Don't print clock messages
  if(msg.key != CKEY)
  {  
    printf("%u %u %u", msg.key, msg.iter, from->u8[0]);
  
    for( i=0; i<DATA_LEN; i++ )
    {
      printf(" %"PRIi64, msg.data[i]);
    }
  
    printf("\n");
  }
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_receiver_process, ev, data)
{
  static struct etimer et;
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();

  broadcast_open(&broadcast, SNIFFER_CHANNEL, &broadcast_call);

  while(1) 
  {
    /* Delay 2-4 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 );

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

