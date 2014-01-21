/*
 * adc.h
 * 
 * Use the A/D converter on the expansion port and broadcast
 * the value of the accelerometer.
 */

#include "adc.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"
//#include "dev/button-sensor.h"

#define MY_CHANNEL 130
#define RESULT_STR_LEN 128

#define SAMPLE_FREQ 8   // Each sample takes 16 chars

/*
 * Global variables
 */
static char result_str[RESULT_STR_LEN-1] = {'\0'};
static uint16_t result_len = 0;
static uint16_t results[3];
static int16_t cdata[3] = {0, 0, 0};              // Calibration constants

/*
 * Local functions.
 */
void calibrate_accel();

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
  static struct etimer et;
  
  PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
  PROCESS_BEGIN();
  
  /*
   * Set up ADC, copied from CCS example
   */
//  WDTCTL = WDTPW+WDTHOLD;                   // Stop watchdog timer
  P6SEL = 0x07;                             // Enable A/D channel inputs
  ADC12CTL0 = ADC12ON+MSC+SHT0_5;           // Turn on ADC12, set sampling time
  ADC12CTL1 = CONSEQ_1 + SHP;               // Sequence of channels
  ADC12MCTL0 = INCH_0;                      // ref+=AVcc, channel = A0
  ADC12MCTL1 = INCH_1;                      // ref+=AVcc, channel = A1
  ADC12MCTL2 = INCH_2+EOS;                  // ref+=AVcc, channel = A2, end seq.
  ADC12IE = 0x04;                           // Enable ADC12IFG.3
  ADC12CTL0 |= ENC;                         // Enable conversions

  broadcast_open(&broadcast, MY_CHANNEL, &broadcast_call);
  //ADC12CTL0 |= ADC12SC;                   // Sampling open
  
  // Calibrate accelerometer.  Wait a second before we do
  etimer_set(&et, CLOCK_SECOND );
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  
  calibrate_accel();
  
  // Light up LED's to signify that calibration is complete.
  leds_on(LEDS_ALL);
  etimer_set(&et, CLOCK_SECOND/2);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  leds_off(LEDS_ALL);
  
  while(1)
  {
    // Only sample data every 1/SAMPLE_FREQ seconds
    if( pause )
    {
      etimer_set(&et, CLOCK_SECOND / (2*SAMPLE_FREQ) );
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      ADC12CTL0 |= ADC12SC;                   // Sampling open
    }
    else
    {
      
      //       puts("----------------");
//      puts(result_str);
//       printf("%d %d %s\n", 
//              result_len, 
//              packetbuf_copyfrom( result_str, result_len+16 ),
//              result_str);
      /*
       * I don't know why we need result_len+16, but without it, the 
       * buffer gets truncated, and packetbuf_copyfrom returns len-15,
       * which it really shouldn't be doing.
       */
      packetbuf_copyfrom( result_str, result_len+16 );
      broadcast_send(&broadcast);
      result_str[0] = 0;
      result_len = 0;
      
    }
    
    pause = (pause+1)%SAMPLE_FREQ;
  }


  PROCESS_END();
}

/*
 * Takes 16 samples, assumed to be in the resting position, and sets the calibration 
 * factors for each dimension.
 */
void calibrate_accel()
{
  static int i, j;
  static int16_t my_cdata[3];
  
  ADC12CTL0 |= ADC12SC;                   // Sampling open
  
  for( j=0; j<3; j++ )
  {
    my_cdata[j] = 0;
  }
  
  // Don't overflow an int_16t!
  // Worst case is results[j] = +/- 2048.
  for( i=0; i<16; i++ )
  {
    while( ADC12CTL1 & ADC12BUSY );
    
    for( j=0; j<3; j++ )
    {
      my_cdata[j] += (results[j] - 2048);
    }
    
//    printf("%d, %d, %d\n", results[0], results[1], results[2]);
    ADC12CTL0 |= ADC12SC;                   // Sampling open
  }
  
  // Divide calibration data by 16 and copy to global cdata
  for( j=0; j<3; j++ )
  {
    cdata[j] = my_cdata[j] >> 4;
  }
//   printf("%d, %d, %d\n", cdata[0], cdata[1], cdata[2]);
}

// ADC12 interrupt service routine
#pragma vector=ADC12_VECTOR
__interrupt void ADC12_ISR (void)
{
  char str[16];
  int len, i;
  
//  putchar('I');
  
  results[0] = ADC12MEM0;                   // Move results, IFG is cleared
  results[1] = ADC12MEM1;                   // Move results, IFG is cleared
  results[2] = ADC12MEM2;                   // Move results, IFG is cleared
  
  // Apply calibrations
  for( i=0; i<3; i++ )
  {
    results[i] -= cdata[i];
  }
  
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
