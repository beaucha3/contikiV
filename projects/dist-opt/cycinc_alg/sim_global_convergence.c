#include <stdio.h>
#include <stdint.h>

#define MAX_ITER 50000
#define NUM_NODES 4

static int32_t glob_iterate(int32_t iterate, int32_t prec_shift, int32_t step);
static int32_t abs_diff32(int32_t a, int32_t b);

static int8_t prec_shift;
static int32_t step;
static int32_t epsilon;


void main()
{
  for(prec_shift = 9; prec_shift <= 16; prec_shift++)
  {
	  for(step = 2; step <= (1 << (prec_shift - 7)); step *= 2)
	  {
		  for(epsilon = 1; epsilon <= step/2; epsilon *= 2)
		  {
			  int i, j;
			  int32_t x = 0, y = 0;
			  
			  //printf("No.,Node,x\n");
			  
			  for(i=0; i < MAX_ITER; i++)
			  {
				y = x;
			    x = glob_iterate(x, prec_shift, step);
			    if(abs_diff32(x, y) <= epsilon && i > 1)
					break;
			
			  }
			  
			  printf("Precision: %u, Step Size: %u, Epsilon: %u, Iteration: %i, Optimum: %i\n", prec_shift, step, epsilon, i, x);
		  }
	  }
  }
}

/*
 * Iterate function that updates data using local gradient 
 */
static int32_t glob_iterate(int32_t iterate, int32_t prec_shift, int32_t step)
{
  return ( iterate - ((step * ( (1 << (2))*iterate - (node_id << (prec_shift + 1)))) >> prec_shift) );
}

/*
 * Returns the absolute difference of two int32_t's, which will
 * always be positive.
 */
int32_t abs_diff32(int32_t a, int32_t b)
{
  int32_t ret;
  
  if( a > b )
    ret = a - b;
  else
    ret = b - a;
  
  return ret;  
}
