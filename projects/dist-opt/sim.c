#include <stdio.h>
#include <stdint.h>

#define PREC_SHIFT 16
#define EPSILON 4

static uint16_t step = 1 << (PREC_SHIFT-3);
static uint8_t stop[4] = {0};

static int32_t grad_iterate(int32_t iterate, int32_t node_id, uint16_t step);
int32_t abs_diff32(int32_t a, int32_t b);


void main()
{
  int i, j;
  int32_t x = 0;
  int32_t y = 0;
  
  printf("No.,Node,x\n");
  i = 1;
  
  while(1)
  {
    for( j=1; j<= 4; j++ )
    {
      y = grad_iterate( x, j , step/(i+1) );
      printf("%i,%i,%i\n",4*i+j,j,y);
      
      if((abs_diff32(y, x) <= EPSILON) && i > 1)
      	  stop[j-1] = 1;
	  else
	  	  x = y;
    }
    
    i++;
    
    if(stop[0] && stop[1] && stop[2] && stop[3])
    	break;
  }
}

static int32_t grad_iterate(int32_t iterate, int32_t node_id, uint16_t step)
{
  return ( iterate - ((step * ( (1 << (node_id + 1))*iterate - (node_id << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
}

int32_t abs_diff32(int32_t a, int32_t b)
{
  int32_t ret;
  
  if( a > b )
    ret = a - b;
  else
    ret = b - a;
  
  return ret;  
}
