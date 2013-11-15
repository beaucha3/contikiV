/*
 * adc.h
 * 
 * Attempt to use the A/D converter on the expansion port and report
 * the value to the shell.
 */

#include "adc.h"
#include <stdio.h>
#include <string.h>

static int adc_value;


PROCESS(main_process, "main");
AUTOSTART_PROCESSES(&main_process);


PROCESS_THREAD(main_process, ev, data)
{
  PROCESS_BEGIN();
  
  /*
   * Set up ADC, copied from CCS example
   */
  ADC12CTL0 = SHT0_2 + ADC12ON;             // Set sampling time, turn on ADC12
  ADC12CTL1 = SHP;                          // Use sampling timer
  ADC12IE = 0x01;                           // Enable interrupt
  ADC12CTL0 |= ENC;                         // Conversion enabled
  P6SEL |= 0x01;                            // P6.0 ADC option select
  P1DIR |= 0x01;                            // P1.0 output

  ADC12CTL0 |= ADC12SC;                   // Sampling open

  printf("Reading ADC...\n");


  PROCESS_END();
}

int read_adc()
{
  printf("%d\n",adc_value);
  
  // Global variable set from interrupt
  return adc_value;
}

// ADC12 interrupt service routine
#pragma vector=ADC12_VECTOR
__interrupt void ADC12_ISR (void)
{
  adc_value = ADC12MEM0;
  read_adc();
}
