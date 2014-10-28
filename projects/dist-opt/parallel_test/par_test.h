#ifndef _PAR_TEST_H_
#define _PAR_TEST_H_

#include <stdint.h>

#define MKEY 1156   // Iterate message key. Chosen by fair die rolls, guaranteed to be random.
#define CKEY 2000   // Round start key, indicates that nodes must update via gradients, and exchange information.
#define AKEY 3000   // Round end key, indicates that nodes must average.
#define NUM_NODES 4   // Number of nodes in grid topology
#define DATA_LEN 1

//Rime constants
#define COMM_CHANNEL 100
#define SNIFFER_CHANNEL 200
#define CLOCK_CHANNEL 300
#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 1

typedef struct opt_message_s
{
  uint16_t key;         // Unique header
  uint16_t iter;         // Number of nodes the iterate has passed through
  int64_t data[DATA_LEN];  // Current data step of optimization algorithm
}
opt_message_t;

typedef struct clock_message_s
{
  uint16_t key;
  int16_t cycle;
  int16_t id;
}
clock_message_t;

//uint16_t u16byteswap(uint16_t x)
//{
	//return ( (x << 8) | (x >> 8));
//}

//int32_t i32byteswap(int32_t x)
//{
	//return ( ((x & 0xFF00FF00) >> 8) | ((x & 0x00FF00FF) << 8) );
//}

#endif /* _PAR_OPT_H_ */
