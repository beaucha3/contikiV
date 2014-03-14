#include <stdio.h>
#include <stdint.h>

#define STEP 128
#define PREC_SHIFT 16

static int32_t grad_iterate(int32_t iterate, int32_t node_id);

void main()
{
  int i, j;
  int32_t x = 0;
  
  printf("No.,Node,x\n");
  
  for( i=0; i<50; i++ )
  {
    for( j=1; j<= 4; j++ )
    {
      x = grad_iterate( x, j );
      printf("%i,%i,%i\n",4*i+j,j,x);
    }
  }
}

static int32_t grad_iterate(int32_t iterate, int32_t node_id)
{
  return ( iterate - ((STEP * ( (1 << (node_id + 1))*iterate - (node_id << (PREC_SHIFT + 1)))) >> PREC_SHIFT) );
}
