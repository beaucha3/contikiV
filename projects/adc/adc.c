/*
 * adc.h
 * 
 * Attempt to use the A/D converter on the expansion port and report
 * the value to the shell.
 */

#include "adc.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"

#define MY_CHANNEL 130
#define RESULT_STR_LEN 128

#define SAMPLE_FREQ 16

static unsigned int results[3];
static char result_str[RESULT_STR_LEN] = {'\0'};
static int result_len = 0;


PROCESS(main_process, "main");
AUTOSTART_PROCESSES(&main_process);
/*---------------------------------------------------------------------------*/
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
  //   printf("broadcast message received from %d.%d: '%s'\n",
  //          from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
  puts( (char *)packetbuf_dataptr() );
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(main_process, ev, data)
{
  static int pause = 1;
  
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();
  
  /*
   * Set up ADC, copied from CCS example
   */
  P6SEL = 0x07;                             // Enable A/D channel inputs
  ADC12CTL0 = ADC12ON+MSC+SHT0_5;           // Turn on ADC12, set sampling time
  ADC12CTL1 = CONSEQ_1;                     // Sequence of channels
  ADC12MCTL0 = INCH_0;                      // ref+=AVcc, channel = A0
  ADC12MCTL1 = INCH_1;                      // ref+=AVcc, channel = A1
  ADC12MCTL2 = INCH_2+EOS;                  // ref+=AVcc, channel = A2, end seq.
  ADC12IE = 0x04;                           // Enable ADC12IFG.3
  ADC12CTL0 |= ENC;                         // Enable conversions

  broadcast_open(&broadcast, MY_CHANNEL, &broadcast_call);
  //ADC12CTL0 |= ADC12SC;                   // Sampling open
  
  while(1)
  {
    static struct etimer et;
    
    // Only sample data every 1/SAMPLE_FREQ seconds
    if( pause )
    {
      etimer_set(&et, CLOCK_SECOND / SAMPLE_FREQ );
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      ADC12CTL0 |= ADC12SC;                   // Sampling open
    }
    else
    {
      packetbuf_copyfrom( result_str, result_len+1 );
      //       puts("----------------");
      puts(result_str);
      
      broadcast_send(&broadcast);
      result_str[0] = 0;
      result_len = 0;
      
    }
    
    pause = (pause+1)%SAMPLE_FREQ;
  }


  PROCESS_END();
}

// ADC12 interrupt service routine
#pragma vector=ADC12_VECTOR
__interrupt void ADC12_ISR (void)
{
  char str[16];
  int len;
  
  putchar('I');
  
  results[0] = ADC12MEM0;                   // Move results, IFG is cleared
  results[1] = ADC12MEM1;                   // Move results, IFG is cleared
  results[2] = ADC12MEM2;                   // Move results, IFG is cleared
  
  len = sprintf(str, "%.4d,%.4d,%.4d\n", 
                results[0], results[1], results[2]);
  
  // Copy to send buffer, will send when we get a chance.
  strncat( result_str, str, RESULT_STR_LEN-result_len-1 );
  
  result_len += len;
  
  if( result_len >= RESULT_STR_LEN )
    result_len = RESULT_STR_LEN - 1;
  
  // This was in the main while loop
  //ADC12CTL0 |= ADC12SC;                   // Sampling open
}
