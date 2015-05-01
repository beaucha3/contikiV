#include "contiki.h"
#include "net/rime.h"
#include <stdio.h>
#include <string.h>
#include "simple-sensor.h"

#define TX 0
/*---------------------------------------------------------------------------*/
PROCESS(broadcast_receiver_process, "Broadcast Receiver");
AUTOSTART_PROCESSES(&broadcast_receiver_process);
/*---------------------------------------------------------------------------*/
static struct broadcast_conn broadcast;

// Recv callback simply prints out received data line by line
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  //printf("broadcast message received from %d.%d: '%s'\n",
          //from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
  
  static data_message_t msg;

  packetbuf_copyto(&msg);
  
  printf("%u\t%u\t%"PRIi64"\n", msg.key, msg.id, msg.data);
}


static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_receiver_process, ev, data)
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

