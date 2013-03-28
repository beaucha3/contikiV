/* CUSUM sequence shell command header file. */

#ifndef __TOOL_SAMPLE_STATS_H__
#define __TOOL_SAMPLE_STATS_H__

#include "shell.h"

// Global declarations
extern uint16_t sample_mean;
extern uint16_t sample_std_dev;

void shell_sample_stats_init(void);

#endif /* __TOOL_SAMPLE_STATS_H__ */
