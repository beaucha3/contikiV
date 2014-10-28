#include "contiki.h"
#include "net/rime.h"
#include <stdio.h>
#include "par_test.h"

#define TX 0
/*---------------------------------------------------------------------------*/
PROCESS(main_process, "Main Process");
AUTOSTART_PROCESSES(&main_process);
/*---------------------------------------------------------------------------*/
static struct broadcast_conn broadcast;
static opt_message_t msg;

static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(main_process, ev, data)
{
  static struct etimer et;
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();

  broadcast_open(&broadcast, SNIFFER_CHANNEL, &broadcast_call);
  msg.key = MKEY;
  msg.iter = 0;
  msg.data[0] = 4096;

  while(1) 
  {
    /* Delay 2-4 seconds */
    etimer_set(&et, CLOCK_SECOND * 2);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        
    msg.iter = msg.iter + 1;
    packetbuf_copyfrom(&msg, sizeof(msg));
    broadcast_send(&broadcast);
  }


  PROCESS_END();
}
/*---------------------------------------------------------------------------*/

