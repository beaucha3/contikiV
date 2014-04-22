/* CUSUM sequence shell command header file. */

#ifndef __CUSUM_SEQ_H__
#define __CUSUM_SEQ_H__

#include "shell.h"

void shell_sequence_init(void);
void collect_data(struct shell_command *c);
void blink_LEDs(unsigned char ledv);

#endif /* __CUSUM_SEQ_H__ */
