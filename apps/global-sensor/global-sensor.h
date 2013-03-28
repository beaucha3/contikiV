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
extern char sensor_sel;

// Function declarations
void shell_global_sensor_init(void);
char shell_datatochar(const char *str);
void sensor_init(void);
void sensor_uinit(void);
uint16_t sensor_read(void);

#endif /* __GLOBAL_SENSOR_H__ */
