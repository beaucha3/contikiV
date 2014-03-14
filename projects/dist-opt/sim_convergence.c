#include <stdio.h>
#include <stdint.h>

#define PREC_SHIFT 16
#define STEP 128
#define EPSILON 128
#define MAX_ITER 50000
#define NUM_NODES 4

static int32_t grad_iterate(int32_t iterate, int32_t node_id);
static int32_t abs_diff32(int32_t a, int32_t b);

void main()
{
  int i, j;
  int32_t x = 0, y = 0;
  
  //printf("No.,Node,x\n");
  
  for(i=0; i < MAX_ITER; i++)
  {
	y = x;
    
    for(j=1; j <= NUM_NODES; j++)
    {
      x = grad_iterate( x, j );
      //printf("%i,%i,%i\n",4*i+j,j,x);
    }
    
    if(abs_diff32(x, y) <= EPSILON && i > 1)
		break;

  }
  
  printf("Precision: %u, Step Size: %u, Epsilon: %u, Iteration: %i, Optimum: %i\n", PREC_SHIFT, STEP, EPSILON, i, x);
}

/*
 * Iterate function that updates data using local gradient 
 */
static int32_t grad_iterate(int32_t iterate, int32_t node_id)
{
  return ( iterate - ((STEP * ( (1 << (node_id + 1))*iterate - (node_id << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
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
