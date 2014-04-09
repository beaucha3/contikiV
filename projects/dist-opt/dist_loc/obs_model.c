/*
 * obs_model.c
 * 
 * Getting model data for distributed light source localization.  
 * 
 * The mote will get a reading from the S1087 sensor and will broadcast the result.
 * 
 */

#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"
