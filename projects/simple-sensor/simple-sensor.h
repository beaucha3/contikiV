#ifndef _SIMPLE_SENSOR_H_
#define _SIMPLE_SENSOR_H_

#include <stdint.h>

#define MKEY 555
#define COMM_CHANNEL 100

typedef struct data_message_s
{
  uint16_t key;         // Message Key
  uint16_t id;         // Address of sender
  int64_t data;  // Data from sensor
}
data_message_t;

#endif /* _SIMPLE_SENSOR_H_ */
