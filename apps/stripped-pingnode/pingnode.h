#ifndef __PINGNODE_H__
#define __PINGNODE_H__

#include "contiki.h"
#include "dev/leds.h"
#include <stdio.h>
#include <stdlib.h>
#include "node-id.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "net/rime.h"
#include <string.h>

struct history_entry
{
	struct history_entry *next;
	rimeaddr_t addr;
	uint8_t seq;
};

#endif /* __PINGNODE_H__ */
