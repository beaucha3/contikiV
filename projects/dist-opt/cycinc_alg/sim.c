#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* 
 * Using fixed step size for now.
 * Actual step size is STEP/256, this is to keep all computations as 
 * integers
 */
#define STEP 2
#define PREC_SHIFT 9
#define START_VAL {30 << PREC_SHIFT, 30 << PREC_SHIFT, 10 << PREC_SHIFT}
#define EPSILON 1       // Epsilon for stopping condition

#define CALIB_C 1     // Set to non-zero to calibrate on reset
#define MODEL_A (48000ll << PREC_SHIFT)
#define MODEL_B (48ll << PREC_SHIFT)
#define MODEL_C model_c
#define SPACING 30      // Centimeters of spacing
#define DATA_LEN 3
#define MAX_ITER 10

/*
 * Arrays to convert Node ID to row/column
 * Lower left node is at (0,0), and arrays are indexed 
 * with NODE_ID
 * 16 <- 15 <- 14
 *  |           |
 * 17 -> 18    13
 *     /        |
 * 10 -> 11 -> 12
 */
int64_t id2row[] = { 0, 0, 0, 1, 2, 2, 2, 1, 1 };
int64_t id2col[] = { 0, 1, 2, 2, 2, 1, 0, 0, 1 };
int64_t data[] = {4608, 5632, 2560, 47616, 28160, 3584, 46080, 32256, 4096};


//Variables for bounding box conditions
static int64_t max_col = (90 << PREC_SHIFT); 
static int64_t max_row = (90 << PREC_SHIFT); 
static int64_t min_col = -1 * (30 << PREC_SHIFT);
static int64_t min_row = -1 * (30 << PREC_SHIFT);
static int64_t max_height = (30 << PREC_SHIFT);
static int64_t min_height = (3 << PREC_SHIFT);  

int64_t get_row( int id );
int64_t get_col( int id );
void grad_iterate(int64_t* iterate, int64_t* result, int len, int id);
int64_t norm2(int64_t* a, int64_t* b, int len);
int64_t g_model(int64_t* iterate, int id);
int64_t f_model(int64_t* iterate, int id);

int main()
{
  int i, j;
  int64_t x[DATA_LEN] = START_VAL;
  int64_t r[DATA_LEN];
  
  printf("No.,Node,col,row,height\n");
  
  for( i=0; i<MAX_ITER; i++ )
  {
    for( j=0; j< 9; j++ )
    {
	  printf("%i,%i,%lli,%lli,%lli\n", 9*i + j, j, x[0], x[1], x[2]);
	  grad_iterate( x, r, DATA_LEN, j );
      memcpy(x, r, DATA_LEN*sizeof(x[0]));
    }
  }
  
  return 0;
}

//~ static int64_t grad_iterate(int64_t iterate, int64_t node_id)
//~ {
  //~ return ( iterate - ((STEP * ( (1 << (node_id + 1))*iterate - (node_id << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
//~ }

void grad_iterate(int64_t* iterate, int64_t* result, int len, int id)
{
  int i;
  
  int64_t node_loc[3] = {get_col(id), get_row(id), 0};
  int64_t reading = data[id];
  
  for(i = 0; i < len; i++)
  {
    int64_t f = f_model(iterate, id);
    int64_t g = g_model(iterate, id);
    int64_t gsq = (g*g) >> PREC_SHIFT;
        
    /*
     * ( MODEL_A * (reading - f) * (iterate[i] - node_loc[i]) ) needs at 
     * most 58 bits, and after the division, is at least 4550.
     */
    result[i] = iterate[i] - ( (4 * STEP * ( ((MODEL_A * (reading - f) * (iterate[i] - node_loc[i])) / gsq) >> PREC_SHIFT)) >> PREC_SHIFT);
    
//     result[i] = iterate[i] - ((((STEP * 4 * (MODEL_A * (reading - f_model(iterate)) / ((g_model(iterate) * g_model(iterate)) >> PREC_SHIFT))) >> PREC_SHIFT) * (iterate[i] - node_loc[i])) >> PREC_SHIFT);
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

int64_t get_row( int id )
{
  return ((id2row[id]) * SPACING) << PREC_SHIFT;
}

/*
 * Returns column of node * spacing in cm
 */
int64_t get_col( int id )
{
  return ((id2col[id]) * SPACING) << PREC_SHIFT;
}

/*
 * Computes the denominator of model
 */
int64_t g_model(int64_t* iterate, int id)
{
  int64_t abh[3];
  
  abh[0] = get_col(id);
  abh[1] = get_row(id);
  abh[2] = 0;
  return ((norm2( iterate, abh, DATA_LEN )) >> PREC_SHIFT) + MODEL_B;
  
  //return ((get_col() - iterate[0])*(get_col() - iterate[0]) + (get_row() - iterate[1])*(get_row() - iterate[1]) + (iterate[2])*(iterate[2])) >> PREC_SHIFT + MODEL_B;
}

/*
 * Computes the observation model function
 */
int64_t f_model(int64_t* iterate, int id)
{
  return (MODEL_A << PREC_SHIFT)/g_model(iterate, id);
}

/*
 * Returns the squared norm of the vectors in a and b.  a and b
 * are assumed to be "shifted" by PREC_SHIFT.
 * Does no bounds checking.
 */
int64_t norm2(int64_t* a, int64_t* b, int len)
{
  int i;
  int64_t retval = 0;
  
  if( a != NULL && b != NULL )
  {
    for( i=0; i<len; i++ )
    {
      retval += (a[i] - b[i])*(a[i] - b[i]);
    }
  }
  
  return retval;
}


