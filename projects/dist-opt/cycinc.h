#ifndef _CYCINC_H_
#define _CYCINC_H_

#include <stdint.h>

#define MKEY 1156   // Chosen by fair die rolls, guaranteed to be random

typedef struct opt_message_s
{
  uint16_t key;         // Unique header
  uint8_t addr[3];      // Allow 3-dimensional addresses, 
                        // for simplicity of expansion to 3-D arrays
  uint8_t iter;         // Iteration of message being passed around
  int32_t data;         // Current result of optimization algorithm
}
opt_message_t;

#endif /* _CYCINC_H_ */
