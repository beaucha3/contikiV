#ifndef __GLOBAL_SENSOR_H__
#define __GLOBAL_SENSOR_H__

#include "shell.h"

/*
External global variable sensor_sel.  This is the currently 'active' sensor.
The one that will be used by any toolkit run (such as mean/std_dev collect.)

v - battery voltage
i - SHT11 battery indicator
l - light1 (photosynthetic) sensor
s - tight2 (total solar) sensor
t - SHT11 temperature sensor
h - SHT11 humidity sensor
*/
extern unsigned char sensor_sel;

// Function declarations
void shell_global_sensor_init(void);
unsigned char shell_datatochar(const unsigned char *str);
int parse_shell_command( unsigned char* cmd, unsigned char* ret );
unsigned char sensor_init(unsigned char s);
unsigned char sensor_uinit(unsigned char s);
uint16_t sensor_read(void);
unsigned char valid_sensor( unsigned char s );

#endif /* __GLOBAL_SENSOR_H__ */
