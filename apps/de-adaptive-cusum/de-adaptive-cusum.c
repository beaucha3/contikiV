#include <stdio.h>
#include "contiki.h"
#include "sky-transmission.h"
#include "sky-math.h"
#include "shell.h"
#include "dev/leds.h"
#include "dev/light-sensor.h"
#include "dev/serial-line.h"
#include "tool-sample-stats.h"

#define NUM_SAMPLES 50
#define ZK_BOUND 35
#define DK_BOUND 7
/*---------------------------------------------------------------------------*/
PROCESS(shell_adaptive_cusum_process, "ad-cusum");
SHELL_COMMAND(adaptive_cusum_command,
              "ad-cusum",
              "ad-cusum: initializes an adaptive cusum test",
              &shell_adaptive_cusum_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(shell_adaptive_cusum_process, ev, data)
{
	PROCESS_BEGIN();
	
	static struct etimer etimer;
//	static struct etimer led_timer;
//	static clock_time_t led_time;
	leds_off(LEDS_ALL);
	
	// delta and epsilon are arbitrarily chosen, will affect how quickly
	// the cusum algorithm converges as well as how noisy it is
	uint16_t delta = 1;
	uint16_t epsilon = 1;

	// phi_min and phi_max are dependent on the ADC of the motes, which is 12-bit
	uint16_t phi_min = 0;
	uint16_t phi_max = 4096;
	static uint16_t phi_hat_a;
	phi_hat_a = sample_mean - delta/2;
	static uint16_t phi_hat_b;
	phi_hat_b = sample_mean + delta/2;
	static uint16_t phi_hat = 0;

	// Standard CUSUM variables
	// Note: threshold "b" is arbitrarily chosen
	static int16_t S_n = 0;
	static int16_t minS_n = 0;
	static uint8_t b = 80;
	static uint8_t detected = 0;


	while (1)
	{
		SENSORS_ACTIVATE(light_sensor);
		etimer_set(&etimer, CLOCK_SECOND/NUM_SAMPLES);
		PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
		uint16_t obs = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
		SENSORS_DEACTIVATE(light_sensor);
		printf("Light reading: %d\n", obs);

/*		if ((clock_time() - led_time > 5 * CLOCK_SECOND)  && detected == 1)
		{
			leds_off(LEDS_RED);
			etimer_set(&etimer, CLOCK_SECOND*5);
			PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
			detected = 0;
		}
*/

	   	// Calculate D_k, essentially a metric of instantaneous change
		int16_t D_k = (mypow2(obs - phi_hat_a))/2 - (mypow2(obs - phi_hat_b))/2;
		
		// Recursion rule
		phi_hat_a = phi_hat_a + epsilon * D_k;
		phi_hat_b = phi_hat_a + delta;

		// Calculate parameter estimate (phi_hat) and check bounds
		phi_hat = phi_hat_a + 0.5 * delta;
		if (phi_hat > phi_max) 
			phi_hat = phi_max;
		else if (phi_hat < phi_min)
			phi_hat = phi_min;
		
		// Run CUSUM test normally, using phi_hat
		// And find new log-likelihood statistic
		int16_t z_k = (mypow2(obs - sample_mean))/2 - (mypow2(obs - phi_hat))/2;
		if (ZK_BOUND * -1 < z_k && z_k < ZK_BOUND)
			z_k = 0;

		// Add the log-likelihood statistic to the current CUSUM
		S_n = S_n + z_k;
	
		// Check to see if a change was detected or not
		if (S_n >= (minS_n + b))
		{
			leds_on(LEDS_RED);
//			etimer_set(&led_timer, 5*CLOCK_SECOND);
			detected = 1;
//			led_time = clock_time();
		} else if (S_n < minS_n)
			minS_n = S_n;
		if (D_k < DK_BOUND * -1 || DK_BOUND < D_k)
			leds_on(LEDS_BLUE);
		else leds_off(LEDS_BLUE);
		printf("S_n: %d\nD_k: %d\n", S_n, D_k);
	}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
void shell_adaptive_cusum_init(void)
{
	shell_register_command(&adaptive_cusum_command);
}
