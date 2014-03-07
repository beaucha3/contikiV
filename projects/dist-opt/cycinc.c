/*
 * cycinc.c
 * 
 * Cyclic Incremental Algorithm Implementation 
 * 
 * The intermediate motes, and they will wait for a message 
 * from the upstream node, compute the local gradient and send it 
 * downstream.
 * 
 * The originator node, node 1, will compute the first gradient and wait
 * for the "go" signal from the master node, node 0.
 * 
 * Subfunctions are hard-coded. Function to optimize is global sum of
 * all subfunctions.
 * 
 */

#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"

#define NODE_ID 2
#define COMM_CHANNEL 100
#define PREC_SHIFT 8

/* 
* Using fixed step size for now.
* Actual step size is STEP/256, this is to keep all computations as 
* integers
* */

#define STEP 1

// Sub-function
static long int grad_iterate(long int iterate)
	return ( iterate - step * ( (1 << (NODE_ID + 1))*x - (NODE_ID << (PREC_SHIFT + 1))))
	
PROCESS(main_process, "main");l
AUTOSTART_PROCESSES(&main_process);
	
	
	
