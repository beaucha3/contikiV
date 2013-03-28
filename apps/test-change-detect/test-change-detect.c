// Lights up the red LED when a change is detected.

#include "test-change-detect.h"
#include "contiki.h"
#include "shell.h"
#include "global-sensor.h"
#include "tool-sample-stats.h"

#include "dev/leds.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdlib.h>
#include "lib/sky-math.h"

#define NUM_DEVS 5

static uint16_t sample;

/*---------------------------------------------------------------------------*/
PROCESS(shell_change_detect_process, "change-detect");
SHELL_COMMAND(change_detect_command,
	      "change-detect",
              "change-detect: starts a change detection process",
              &shell_change_detect_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(shell_change_detect_process, ev, data)
{
	static struct etimer etimer;

	PROCESS_BEGIN();
	
	leds_init();
	while(1) {
		sensor_init();
		etimer_set(&etimer, CLOCK_SECOND / 4);
		PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
		sample = sensor_read();
		sensor_uinit();
		printf("sample = %d\n",sample);
		
		if(abs_sub(sample, sample_mean) > (sample_std_dev * NUM_DEVS)) {
			// Change detected, turn on LED(s)?
			leds_on(LEDS_RED);
		} else {
			// Turn off LED(s).
			leds_off(LEDS_RED);
		}
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void shell_test_change_detect_init(void)
{
	shell_register_command(&change_detect_command);
}
