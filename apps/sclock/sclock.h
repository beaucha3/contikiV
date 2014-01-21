/*
 * Header file for sclock app.  See sclock.c for more information.
 */

#ifndef SCLOCK_H
#define SCLOCK_H

/*
 * Channel and commands defined in dan_protocol.h
 */
#include net/rime/dan_protocol.h

/*
 * Exported functions -
 *    sclock_initialize()     - initialize the local sclock
 *    sclock_reset()          - reset the local sclock to zero
 *    sclock_time()           - get the current sclock time in timer ticks (1/128 seconds)
 *    sclock_open_channel()   - open a broadcast channel for the sclock
 *    sclock_close_channel()  - close the broadcast channel
 */

void sclock_initialize();
void sclock_reset();
unsigned long int sclock_time();
void sclock_open_channel();
void sclock_close_channel();



#endif /* SCLOCK_H */
