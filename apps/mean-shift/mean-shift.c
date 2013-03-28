/* This is a very simple change detection algorithm. It will take 50 data samples
 * to begin and use as a baseline. The mean and standard deviation will be calculated
 * from this sample set. Thereafter, the main algorithm will take 50 samples every 
 * second and run the same tests. If the mean has shifted by at least 3 standard
 * deviations, a change has occurred, and... something will happen. Promise.
 * Author: Dario Aranguiz
 */

#include "mean-shift.h"
#include "contiki.h"
#include "net/rime.h"
#include "net/rime/mesh.h"
#include "shell.h"
#include "shell-collect-view.h"
#include "collect-view.h"

#include "dev/leds.h"
#include "dev/light-sensor.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_SAM 20

/*---------------------------------------------------------------------------*/
static struct mesh_conn mesh;

static void
sent(struct mesh_conn *c)
{
  printf("packet sent\n");
}

static void
timedout(struct mesh_conn *c)
{
  printf("packet timedout\n");
}

static void
recv(struct mesh_conn *c, const rimeaddr_t *from, uint8_t hops)
{
  printf("Data received from %d.%d: %s (%d)\n",
         from->u8[0], from->u8[1],
         (char *)packetbuf_dataptr(), packetbuf_datalen());
}

const static struct mesh_callbacks callbacks = {recv, sent, timedout};
/*---------------------------------------------------------------------------*/
uint16_t counter;
static uint16_t mean_0 = 0;
static int16_t mean_1;
static uint16_t stdev_0 = 1;

/*---------------------------------------------------------------------------*/
PROCESS(shell_simpledetect_process, "simple-detect");
SHELL_COMMAND(simpledetect_command,
	      "simple-detect",
              "simple-detect: starts a new simple-detect sequence",
              &shell_simpledetect_process);

PROCESS(shell_sample_init_process, "sample-init");
SHELL_COMMAND(sample_init_command,
              "sample-init",
              "sample-init: gathers initial condition stats",
              &shell_sample_init_process);

PROCESS(shell_mesh_reset_process, "mesh-reset");
SHELL_COMMAND(mesh_reset_command,
              "mesh-reset",
              "mesh-reset: reopens the mesh connection",
              &shell_mesh_reset_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(shell_simpledetect_process, ev, data)
{
	static struct etimer etimer;
	static uint16_t data_sam[NUM_SAM];
	char message[52];
	char numhold[5];

	PROCESS_EXITHANDLER(mesh_close(&mesh);)

	PROCESS_BEGIN();

	SENSORS_ACTIVATE(light_sensor);
	etimer_set(&etimer, CLOCK_SECOND);
	PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
	while (1)
	{
		blink_LEDs(LEDS_ALL);
		mean_1 = 0;

		for (counter = 0; counter < NUM_SAM; counter++)
		{
			etimer_set(&etimer, CLOCK_SECOND / NUM_SAM);
			PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
			data_sam[counter] = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
			mean_1 = mean_1 + data_sam[counter];
		}

		mean_1 = mean_1/NUM_SAM - mean_0;
                if (mean_1 < 0)
                        mean_1 = mean_1 * -1;
                
		rimeaddr_t addr;
                addr.u8[0] = 62;
                addr.u8[1] = 41;
                strcpy(message, "\nmean_0 = "); // 10char
                itoa(mean_0, numhold, 10);
                strcat(message, numhold);  // 5 char
                strcat(message, "\ndifference = "); // 14 char
                itoa(mean_1, numhold, 10);
		strcat(message, numhold); // 5 char
		strcat(message, "\nst. dev. = "); // 12 char
		itoa(stdev_0, numhold, 10);
		strcat(message, numhold); // 5 char
		strcat(message, "\n"); // 1 char                Total: 51 chars
		
		packetbuf_copyfrom(message, sizeof(message));
		mesh_send(&mesh, &addr);
		
		if (mean_1 > stdev_0*3)
		{
			leds_on(LEDS_RED);
			packetbuf_copyfrom("Change detected", strlen("Change detected"));
			sensor_uinit();
			mesh_send(&mesh, &addr);
			break;
		}
	}

	PROCESS_END();	
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(shell_sample_init_process, ev, data)
{
	static uint16_t data_sam[NUM_SAM];
	static struct etimer etimer;

	PROCESS_BEGIN();
	
	
	// Gather NUM_SUM samples over one second of time
	SENSORS_ACTIVATE(light_sensor);
	mean_0 = 0;
	for (counter = 0; counter < NUM_SAM; counter++)
	{
		etimer_set(&etimer, CLOCK_SECOND / NUM_SAM); 
		PROCESS_WAIT_UNTIL(etimer_expired(&etimer));
		data_sam[counter] = light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC);
		mean_0 = mean_0 + data_sam[counter];
		printf("Reading = %d  Sum = %d\n", data_sam[counter], mean_0);
	}
	SENSORS_DEACTIVATE(light_sensor);

	mean_0 = mean_0/NUM_SAM;
	stdev_0 = mystdev(data_sam, mean_0);

	PROCESS_END();
}

PROCESS_THREAD(shell_mesh_reset_process, ev, data)
{
	PROCESS_BEGIN();
	mesh_open(&mesh, 132, &callbacks);
	PROCESS_END();
}

/*---------------------------------------------------------------------------*/
void shell_mean_shift_init(void)
{
	mesh_open(&mesh, 132, &callbacks);
	shell_register_command(&simpledetect_command);
	shell_register_command(&sample_init_command);
	shell_register_command(&mesh_reset_command);
}

// Does a squre root operation on num.
uint16_t mysqrt(uint16_t num)
{
	uint16_t y = 1;
	for (counter = 0; counter < 20; counter++)
	{
		y = (num/y + y) / 2;
		printf("cur. st. dev. = %d\n", y);
	}
	printf("st. dev. = %d\n", y);
	return y;
}

uint16_t mystdev(uint16_t *sample_array, uint16_t mean)
{
	uint16_t sum = 0;
	printf("Mean = %d\n", mean);
	for (counter = 0; counter < NUM_SAM; counter++)
	{
		sum = (sample_array[counter] - mean) * (sample_array[counter] - mean) + sum;
		printf("Light reading = %d\nDifference = %d\nSum = %d\n", sample_array[counter],
			sample_array[counter] - mean, sum);
	}
	sum = sum / (NUM_SAM - 1);
        return mysqrt(sum);
}

// 1/8th second LED blink
// So what's going on here? With leds_off, ledv, current_leds? 
void blink_LEDs(unsigned char ledv) 
{
	unsigned char current_leds;
	current_leds = leds_get();
	leds_on(ledv);
	clock_delay(250);
	if (current_leds == LEDS_RED)
		leds_off(ledv & (~LEDS_RED));
	else leds_off(ledv);
}

