#ifndef _PAR_OPT_H_
#define _PAR_OPT_H_

#include <stdint.h>

#define MKEY 1156   // Iterate message key. Chosen by fair die rolls, guaranteed to be random.
#define CKEY 2000   // Chosen by grad student, not necessarily random.
#define MAX_NODES 9     // Number of nodes in topology
#define COMM_CHANNEL 100
#define SNIFFER_CHANNEL 200
#define CLOCK_CHANNEL 300
#define DATA_LEN 3

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
}
clock_message_t

//uint16_t u16byteswap(uint16_t x)
//{
	//return ( (x << 8) | (x >> 8));
//}

//int32_t i32byteswap(int32_t x)
//{
	//return ( ((x & 0xFF00FF00) >> 8) | ((x & 0x00FF00FF) << 8) );
//}

#endif /* _PAR_OPT_H_ */
