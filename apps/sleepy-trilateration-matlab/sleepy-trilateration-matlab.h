#ifndef __SLEEPY_TRILATERATION_MATLAB_H__
#define __SLEEPY_TRILATERATION_MATLAB_H__

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
#include "sky-math.h"
#include "global-sensor.h"
#include "net/rime.h"
#include <string.h>

void shell_sleepy_trilateration_matlab_init();

struct history_entry
{
	struct history_entry *next;
	rimeaddr_t addr;
	uint8_t seq;
};

#endif /* __SLEEPY_TRILATERATION_MATLAB_H__ */
