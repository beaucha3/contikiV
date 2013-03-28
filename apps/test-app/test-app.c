#include "test-app.h"
#include "dev/leds.h"
#include "dev/watchdog.h"
#include "stdio.h"

/*---------------------------------------------------------------------------*/
PROCESS(shell_test_app_process, "test-app");
SHELL_COMMAND(test_command,
              "test-app",
              "test-app: runs a test app",
              &shell_test_app_process);
/*---------------------------------------------------------------------------*/
struct blinker {
	struct pt blink_pt;
	struct etimer etimer;
	int led;
	int i;
};



static
PT_THREAD(blink(struct blinker *b))
{
	PT_BEGIN(&b->blink_pt);

	printf("PT Started\n");
//	PT_YIELD(&b->blink_pt);
	watchdog_stop();
	
	for (b->i = 0; b->i < 100; b->i++)
	{
		//etimer_set(&b->etimer, 3 * CLOCK_SECOND);
		leds_on(b->led);
		//PT_WAIT_UNTIL(&b->blink_pt, etimer_expired(&b->etimer));	
		clock_delay(4000);
		leds_off(b->led);
		clock_delay(4000);
		printf("Iterated");
	}
	
	watchdog_start();
	printf("PT Ended\n");

	PT_END(&b->blink_pt);
}

PROCESS_THREAD(shell_test_app_process, ev, data)
{
	PROCESS_BEGIN();
/*	static struct blinker red, green, blue;	
	red.led = LEDS_RED;
	green.led = LEDS_GREEN;
	blue.led = LEDS_BLUE;

	PT_INIT(&red.blink_pt);
	PT_INIT(&blue.blink_pt);
	PT_INIT(&green.blink_pt);

	PT_SCHEDULE(blink(&red));
	PT_SCHEDULE(blink(&blue));
	PT_SCHEDULE(blink(&green));
*/
	static struct etimer etimer;
	etimer_set(&etimer, CLOCK_SECOND*2);
	leds_on(LEDS_ALL);
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&etimer));
	leds_off(LEDS_ALL);
	
	watchdog_stop();	
	__bis_SR_register(LPM3_bits + GIE);

	printf("Process ended\n");
	PROCESS_END();
}
	
/*---------------------------------------------------------------------------*/
void shell_test_app_init()
{
	shell_register_command(&test_command);
}

