#include "contiki.h"
#include "net/rime.h"

#define TX 0

#include <stdio.h>

//#include "shell.h"
#include "cycinc.h"
/*---------------------------------------------------------------------------*/
PROCESS(runicast_receiver_process, "Runicast Receiver");
AUTOSTART_PROCESSES(&runicast_receiver_process);
/*---------------------------------------------------------------------------*/
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

static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
  //printf("broadcast message received from %d.%d: '%s'\n",
          //from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

  static opt_message_t msg;
  int i;

	packetbuf_copyto(&msg);
  
  printf("%u %u", msg.iter, from->u8[0]);
  
  for( i=0; i<DATA_LEN; i++ )
  {
    printf(" %"PRIi64, msg.data[i]);
  }
  
  printf("\n");
  
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

//static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct runicast_callbacks runicast_callbacks = 
{ 
  recv_runicast,
  sent_runicast,
  timedout_runicast
};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(runicast_receiver_process, ev, data)
{
  static struct etimer et;
  PROCESS_EXITHANDLER(runicast_close(&runicast);)
  PROCESS_BEGIN();

  runicast_open(&runicast, COMM_CHANNEL, &runicast_callbacks);

  while(1) 
  {
    /* Delay 2-4 seconds */
    etimer_set(&et, CLOCK_SECOND * 4 );

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

