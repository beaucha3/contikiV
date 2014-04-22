/*
 * sclock app
 * Dan Whisman (whisman1)
 *
 * This app aims to synchronize the local clocks of a network of sensor motes.
 * Each mote has a clock module, which gives us 128 ticks per second on the msp430 based boards,
 * or sky motes.  One mote will act as the control, and will send a broadcast message to all the
 * other motes on TIME_CHANNEL.  When receiving this message, all motes will reset their time to 0.
 *
 * The control mote will be, for now, connected to a PC, so the reset/initialization is done via
 * shell command.
 *
 * Upon receiving the message to reset, each mote will set start_time to the current time.  The
 * time is then determined by subtracting the current time from the system clock - returned by
 * clock_time() - so the time is given in timer ticks, or 1/128 second intervals.
 *
 * Note that since there is some delay in the reset/initialize message, due to network latency,
 * the timers will not be exactly synchronous, but should be good enough for our purposes.
 */

#include "sclock.h"
#include "sys/clock.h"

#include "net/rime.h"
#include "shell.h"

#include <limits.h>

// For debugging
#include <stdio.h>

#define DEBUG


/*
 * Static Global Data
 */
unsigned char is_initialized = 0;
unsigned char channel_open = 0;
unsigned long int start_time = ULONG_MAX;

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;

/*
 * Local Functions
 */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from);

/*
 * Initializes the sclock
 * Sets is_initialized to 1 and resets start_time to current_time
 * Assumes clock_time returns a valid time - there is nothing in the Contiki documentation
 * to suggest otherwise.
 * Does nothing if sclock already initialized.
 */
void sclock_initialize()
{
  if( is_initialized == 0 )
  {
    is_initialized = 1;
    sclock_reset();
  }
}


/*
 * Resets the sclock
 * Sets start_time to current_time
 * Assumes clock_time returns a valid time - there is nothing in the Contiki documentation
 * to suggest otherwise.
 * Does nothing if sclock not initialized.
 */
void sclock_reset();
{
  if( is_initialized )
  {
    start_time = clock_time();
  }
}


/*
 * Gets the current sclock time if initialized, otherwise, returns ULONG_MAX.
 * If clock_time() < start_time, returns difference anyway because clock_time() has looped around.
 * Assumes clock_time returns a valid time - there is nothing in the Contiki documentation
 * to suggest otherwise.
 */
unsigned long int sclock_time()
{
  unsigned long int ret;

  if( is_initialized )
  {
    ret = clock_time() - start_time;

    /*
     * ULONG_MAX is used to denote an error, so we don't want to return it if everything is fine.
     * We will be off by one timer tick, but that is acceptable.
     * Err on the side of a smaller time value.
     */
    if( ret == ULONG_MAX )
      ret--;
  }
  else
  {
    ret = ULONG_MAX;
  }

  return ret;
}

/*
 * Handler for broadcast messages
 */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  // Check if this is the correct channel
  if( c->c.channel.channelno == TIME_CHANNEL )
  {
    char* rdata = (char *)packetbuf_dataptr();

    switch( rdata[0] )
    {
      case TIME_INIT:
        sclock_initialize();
        break;

      case TIME_RESET:
        sclock_reset();
        break;

      default:
        printf("Unknown command received from %d.%d: '0x%X'\n",
              from->u8[0], from->u8[1], rdata[0]);
        break;
    }
  }
}

/*
 * Opens channel for broadcast messages.
 */
void sclock_open_channel()
{
  if( channel_open == 0 )
  {
    broadcast_open(&broadcast, TIME_CHANNEL, &broadcast_call);
    channel_open = 1;
  }
}

/*
 * Closes channel for broadcast messages.
 */
void sclock_close_channel()
{
  if( !(channel_open == 0) )
  {
    broadcast_close(&broadcast);
    channel_open = 0;
  }
}


