/*

File: Header file for the air quality monitoring project.  This includes
various libraries and defines macros for ease of programming.


Author: Nathan Beauchamp (beaucha3@illinois.edu)

with assistance from Isaac Kousari (kousari2@illinois.edu)

Institution: University of Illinois at Urbana-Champaign, Coordinated Science Lab
Affiliation: Undergraduate Research Assistants under Professor Veeravalli
Special Thanks to: Neeraj Venkatesan, UIUC M.S. EE graduate

*/

#ifndef AIR_QUALITY_H /* Include guards prevent multiple declarations of static types/variables */
#define AIR_QUALITY_H /* Could use #pragma once instead, but that doesn't work for all compiler distributions */


/* Include libraries! */

#include "contiki.h" /* Main contiki library */
#include "shell.h"
#include "serial-shell.h"
#include "lib/sensors.h"
#include "lib/memb.h"
#include "lib/list.h"
#include "net/rime.h" /* Communication stack */
#include "dev/leds.h" /* Libraries for sensors */
#include "dev/button-sensor.h"
#include "dev/sky-sensors.h"
#include "dev/temperature-sensor.h"
#include "dev/sht11-sensor.h"
#include "sys/etimer.h"
#include "node-id.h"
#include "dev/sht11-arch.h" /* Architecture-specific definitions for the temperature/humidity sensor */
#include <stdio.h> /* Standard C libraries for i/o, math, strings, etc. */
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

/* Definition of constants relating to communications and memory allocation */

#define NUM_MOTES 9 /* Motes will be arranged in a 3x3 grid */
#define NUM_NEIGHBORS 4 /* In a rectangular grid, the maximum number of neighbors is 4 */
#define COMM_CHANNEL 100 /* Hardware-specific */
#define SNIFFER_CHANNEL 129
#define M_KEY 555
#define MAX_ROWS 3 /* There will be three rows of motes, indexed from 0 to MAX_ROWS-1 */
#define MAX_COLS 3 /* There will be three columns of motes, indexed from 0 to MAX_COLS-1 */
#define START_ID 10 /* ID of the first node in the chain */
#define NODE_ID (rimeaddr_node_addr.u8[0]) /* Hardcoded mote address */
#define NORM_ID (rimeaddr_node_addr.u8[0] - START_ID) /* Normalized ID enables us to reference motes as 0-8 */
#define SNIFFER_NODE_0 23 /* Hardcoded sniffer node address */
#define SNIFFER_NODE_1 0
#define SEQNO_EWMA_UNITY 0x100 /* These two defines are used to compute the moving average of seqno gaps */
#define SEQNO_EWMA_ALPHA 0x040
#define PERIOD 2 /* Number of seconds between each data collection */

/* Definition of constants relating to the diffusion mathematical model */

#define K_BOLT (1.3806488 * pow(10, -23)) /* Boltzmann constant in kg m^2 s^-2 K^-1 */

#ifndef M_PI
#define M_PI 3.14159265358979323846 /* C99 standard does not provide an M_PI definition */
#endif

#define N_AVO (6.022 * pow(10, 23)) /* Avogadro's number (necessary for getting molecular mass from molar mass) */
#define MA_H2O 18.01528 /* Molar mass of water in g/mol. We're examining diffusion of water (A) through air (B) */

/* "Molar mass" of dry air in g/mol. Computed via an average of the masses of gases present in the air, weighted by their concentrations: see http://www.engineeringtoolbox.com/molecular-mass-air-d_679.html */

#define MB_Air 28.97

#define P_ATMOS 1.00 /* Atmospheric pressure is 1 atm */

/* Molecular diameters */

#define DA_H20 (2.9 * pow(10,-10)) /* The molecular diameter of water (gas A) in meters */
#define DB_N2 (3.7 * pow(10,-10)) /* The molecular diameter of nitrogen in meters */
#define DB_O2 (3.58 * pow(10,-10)) /* The molecular diameter of oxygen in meters */
#define DB_AR (3.4 * pow(10,-10)) /*  The molecular diameter of argon in meters */
#define DB_CO2 (4.07 * pow(10,-10)) /* The molecular diameter of carbon dioxide in meters */

#define DB_AIR (get_air_diam()) /* This macro will be replace "DB_AIR" with a function call that returns the average diameter of an air molecule. The function itself is implemented in the main source file and relies on other macro definitions */

/* Definition of the concentrations of gases in air */

#define N2_CONC 0.7809
#define O2_CONC 0.2095
#define AR_CONC 0.00933
#define CO2_CONC 0.0003

/*
*	Projected arrangement of nodes. Top left node is at (0,0), and arrays are indexed 
* 	with NODE_ID. Full grid topology, not single cycle. All comm links are bi-directional.
*
*	10 - 11 - 12
*	 |    |    |
*	13 - 14 - 15
*	 |    |    |
*	16 - 17 - 18
*
*/


/* Useful structure definitions */

typedef struct data_message_s /* Structure for messages sent via broadcast  */
{
	uint16_t key; /* Message key */
	int16_t temp; /* Temperature data from sensor */
	int16_t humid; /* Humidity data from sensor */

	float Xopt; /* Transceiving node's estimate for the source location */
	float Yopt;

	uint8_t h; /* Number of hops from transceiving node to a participating node */
	bool b; /* Boolean variable indicating participation */
	
	uint8_t seqno; /* Measure of the number of broadcast packets sent so far */

`	unsigned char addr; /* Address of the node from which message was sent (least significant bits) */
} 
data_message_t;

typedef struct node_s /* Structure defining node relations. Each node can have up to 4 neighbors */
{
	struct node_s *next; /* Used in forming a linked list */

	struct node_s *left;
	struct node_s *right;
	struct node_s *top;
	struct node_s *bottom;

	rimeaddr_t addr; /* Rime address of the node in question */

	/* This node's estimates for optimal source position (i.e. position such that the squared difference between the sensor reading and the expected sensor reading based on node position and estimated source position is minimized) */
	
	float Xopt;
	float Yopt;

	uint8_t h; /* Number of hops from this node to a participating node */
	bool b; /* Boolean variable indicating participation */

	/* Hold the Received Signal Strength Indicator (RSSI) and Link Quality Indicator (LQI) values that are 		received from broadcast packets outgoing from this node */
	uint32_t last_rssi, last_lqi;
	
	uint8_t last_seqno; /* Last sequence number a neighbor saw from this node */
	uint8_t seqno_gap; /* Average sequence number gap a neighbor saw from this node */

}
node_t;

#endif /* For AIR_QUALITY_H */


