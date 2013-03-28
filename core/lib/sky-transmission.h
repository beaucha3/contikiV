#ifndef __SKY_TRANSMISSION_H__
#define __SKY_TRANSMISSION_H__

#include "net/rime.h"
#include <stdio.h>
#include <string.h>

extern uint8_t from_node;

int transmit_mesh(char *message, uint8_t addr_one);
void open_mesh();
void close_mesh();

int transmit_broadcast(char *message);
void open_broadcast();
void close_broadcast();

int transmit_unicast(char *message, uint8_t addr_one);
void open_unicast();
void close_unicast();

#endif /* __SKY_TRANSMISSION_H__ */
