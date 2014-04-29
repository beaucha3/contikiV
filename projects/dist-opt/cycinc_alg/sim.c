#include <stdio.h>
#include <stdint.h>

#define STEP 2
#define PREC_SHIFT 9

//Variables for bounding box conditions
static int64_t max_col = (90ll << PREC_SHIFT); 
static int64_t max_row = (90ll << PREC_SHIFT); 
static int64_t min_col = -1 * (30ll << PREC_SHIFT);
static int64_t min_row = -1 * (30ll << PREC_SHIFT);
static int64_t max_height = (30ll << PREC_SHIFT);
static int64_t min_height = (3ll << PREC_SHIFT);  


static int64_t grad_iterate(int64_t iterate, int64_t node_id);

void main()
{
  int i, j;
  int64_t x = STEP;
  
  printf("No.,Node,x\n");
  
  for( i=0; i<300; i++ )
  {
    for( j=1; j<= 9; j++ )
    {
      x = grad_iterate( x, j );
      printf("%i,%i,%i\n",4*i+j,j,x);
    }
  }
}

//~ static int64_t grad_iterate(int64_t iterate, int64_t node_id)
//~ {
  //~ return ( iterate - ((STEP * ( (1 << (node_id + 1))*iterate - (node_id << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
//~ }

static void grad_iterate(int64_t* iterate, int64_t* result, int len)
{
  int i;
  
  int64_t node_loc[3] = {get_col(), get_row(), 0};
  int64_t reading = (light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC) << PREC_SHIFT) - MODEL_C;
  
  for(i = 0; i < len; i++)
  {
    int64_t f = f_model(iterate);
    int64_t g = g_model(iterate);
    int64_t gsq = (g*g) >> PREC_SHIFT;
    
    /*
     * ( MODEL_A * (reading - f) * (iterate[i] - node_loc[i]) ) needs at 
     * most 58 bits, and after the division, is at least 4550.
     */
    result[i] = iterate[i] - ( (4ll * STEP * ( ((MODEL_A * (reading - f) * (iterate[i] - node_loc[i])) / gsq) >> PREC_SHIFT)) >> PREC_SHIFT);
    
//     result[i] = iterate[i] - ((((STEP * 4ll * (MODEL_A * (reading - f_model(iterate)) / ((g_model(iterate) * g_model(iterate)) >> PREC_SHIFT))) >> PREC_SHIFT) * (iterate[i] - node_loc[i])) >> PREC_SHIFT);
  }
  
  /*
   * Bounding Box conditions to bring the iterate back if it strays too far 
   */
   //printf("result[0] = %"PRIi64" result[1] = %"PRIi64" result[2] = %"PRIi64"\n", result[0], result[1], result[2]);
   //printf("max_col = %"PRIi64" min_col = %"PRIi64" max_row = %"PRIi64" min_row = %"PRIi64" max_height = %"PRIi64" min_height = %"PRIi64"\n", max_col, min_col, max_row, min_row, max_height, min_height);
   if(result[0] > max_col)
   {
	   result[0] = max_col;
   }
   
   if(result[0] < min_col)
   {
	   result[0] = min_col;
   }

   if(result[1] > max_row)
   {
	   result[1] = max_row;
   }
   
   if(result[1] < min_row)
   {
     result[1] = min_row;
   }
   
   if(result[2] > max_height)
   {
	   result[2] = max_height;
   }
	  
   if(result[2] < min_height)
   {
	   result[2] = min_height;
   } 
  
}
