#ifndef _RP_BCAST_H_
#define _RP_BCAST_H_

#include <stdint.h>

#define TKEY 2000   // Tick message key.
#define NUM_NODES 9   // Number of nodes in grid topology
#define DATA_LEN 3

//Rime constants
#define COMM_CHANNEL 100
#define SNIFFER_CHANNEL 200

typedef struct opt_message_s
{
  uint16_t key;         // Unique header
  uint16_t iter;         // Number of nodes the iterate has passed through
  int64_t data[DATA_LEN];  // Current data step of optimization algorithm
  int64_t sensor_val; // Most recent sensor value reading
  uint16_t hop_number; //Number of hops from a participating node
  uint16_t node;
}
opt_message_t;

//uint16_t u16byteswap(uint16_t x)
//{
	//return ( (x << 8) | (x >> 8));
//}

//int32_t i32byteswap(int32_t x)
//{
	//return ( ((x & 0xFF00FF00) >> 8) | ((x & 0x00FF00FF) << 8) );
//}

#endif /* _PAR_OPT_H_ */
