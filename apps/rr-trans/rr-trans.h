#ifndef __RR_TRANS_H__
#define __RR_TRANS_H__

#include "shell.h"
#include "contiki.h"
#include "dev/leds.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdlib.h>
#include "node-id.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "sky-transmission.h"
#include "global-sensor.h"
#include "net/rime.h"
#include <string.h>

void shell_rr_trans_init(void);

struct history_entry
{
	struct history_entry *next;
	rimeaddr_t addr;
	uint8_t seq;
};

#endif /* __RR_TRANS_H_ */
