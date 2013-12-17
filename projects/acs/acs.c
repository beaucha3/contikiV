/*
 * adc.h
 * 
 * Use the A/D converter on the expansion port and broadcast
 * the value of the accelerometer.
 */

#include "acs.h"
#include "contiki.h"
#include <stdio.h>
#include <string.h>
#include "net/rime.h"
#include "dev/leds.h"
//#include "dev/button-sensor.h"

#define MY_CHANNEL 130
#define RESULT_STR_LEN 128

/*
 * The two constants below define how often data is sampled (SAMPLE_FREQ) and how
 * large the outgoing packets are (SEND_FREQ).
 * Note: the packet buffer is only 128 bytes, so set these so that no more than that
 * are sent at once.
 */
#define SAMPLE_FREQ 16  // (Approximate) frequency of data sampling
#define SEND_FREQ 4     // Number of samples to send at a time, larger = less frequent


/*
 * Parameters for CuSum
 */
#define PREC      11      // Bit shift for precesion (11 = 2048)
#define MU0       3547    // Mean of pre-change (rest) distribution
#define SIG0      45000   // 2*variance of pre-change distribution
#define MU1       3157    // Mean of post-change (free-fall) distribution
#define SIG1      208     // 2*variance of post-change distribution
#define LOG_S0_S1 5506    // 2048*ln(stdev0/stdev1)

#define THRESHOLD 40000   // This should detect a change within 4 samples

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
  
  char str[16];
  int len;
  
  static int32_t Wn = 0;      // Significant statistic
  uint16_t mag;               // Magnitude of acceleration
  
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
      ADC12CTL0 |= ADC12SC;                   // Sampling open
      etimer_set(&et, CLOCK_SECOND / SAMPLE_FREQ );
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
      
      // Calculate the magnitude of the acceleration
      // Use square root for now, but may not need later
      mag = results[0]*results[0] + 
            results[1]*results[1] + 
            results[2]*results[2];
      
      // Take the square root of mag - which may be as high as 3*(2^24).
      // Currently, I can't do this, I'll need to look at my Crenshaw book
      
      // Update significant statistic
      Wn += LOG_S0_S1 + (((mag - MU0)*(mag - MU0)) << 11)/SIG0
                      - (((mag - MU1)*(mag - MU1)) << 11)/SIG1;
                      
      if( Wn < 0 )
      {
        Wn = 0;
      }
      
      if( Wn > THRESHOLD )
      {
        // Change detected, light up LED's
        leds_on(LEDS_ALL);
      }
      else
      {
        // No change
        leds_off(LEDS_ALL);
      }
      
      // Copy significant statistic and acceleration magnitude to buffer
      len = sprintf(str, "%ld,%ld\n", Wn, mag);
      
      // Copy to send buffer, will send when we get a chance.
      strncat( result_str, str, RESULT_STR_LEN-result_len-1 );
      
      result_len += len;
      
      if( result_len >= RESULT_STR_LEN )
        result_len = RESULT_STR_LEN - 1;
    }
    else
    {
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
    
    pause = (pause+1)%SEND_FREQ;
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
  int i;
  
  results[0] = ADC12MEM0;                   // Move results, IFG is cleared
  results[1] = ADC12MEM1;                   // Move results, IFG is cleared
  results[2] = ADC12MEM2;                   // Move results, IFG is cleared
  
  // Apply calibrations
  for( i=0; i<3; i++ )
  {
    results[i] -= cdata[i];
  }
}
