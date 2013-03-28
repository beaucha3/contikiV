#ifndef __FUSION_RR_H__
#define __FUSION_RR_H__

#include "shell.h"
#include "contiki.h"
#include "dev/leds.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdlib.h>
#include "node-id.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "net/rime.h"
#include <string.h>

void shell_fusion_rr_init(void);

struct history_entry
{
	struct history_entry *next;
	rimeaddr_t addr;
	uint8_t seq;
};

#endif /* __FUSION_RR_H_ */
