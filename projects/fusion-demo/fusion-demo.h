#ifndef __FUSION_DEMO_H__
#define __FUSION_DEMO_H__

#include "contiki.h"
#include "dev/leds.h"
#include "dev/light-sensor.h"
#include "dev/button-sensor.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdlib.h>
#include "node-id.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "sky-transmission.h"
#include "sky-math.h"
#include "net/rime.h"
#include <string.h>


struct history_entry
{
	struct history_entry *next;
	rimeaddr_t addr;
	uint8_t seq;
};

#endif /* __FUSION_DEMO_H__ */
