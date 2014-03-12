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

	static opt_message_t msg_recv;
	static opt_message_t* msg = &msg_recv;	

	packetbuf_copyto(msg);
	
	printf("Received Packet: Key: %"PRIu16"\tNode: %"PRIu8"\tIter: %"PRIu8"\tData: %"PRIi32"\n", msg->key, msg->addr[0], msg->iter, msg->data);
	
	//printf("Recv handler got something\n");
	
	//int i = 0;
	//for(i ; i < packetbuf_datalen(); i++)
	//{
		//printf("%0.2X ", *((opt_message_t*)packetbuf_dataptr() + i));
	//}
	//printf("\n");	
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

