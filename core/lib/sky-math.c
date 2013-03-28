#include "lib/sky-math.h"
#include <stdio.h>

#define NUM_TERMS 16
#define PRECISION 11

#if PRECISION == 11
unsigned short log_table[NUM_TERMS] = { 7098, //ln(32)
                             5678, //ln(16)
                             4259, //ln(8)
							 2839, //ln(4)
							 1420, //ln(2)
							 830, //ln(3/2)
							 457, //ln(5/4)
							 241, //ln(9/8)
							 124, //ln(17/16)
							 63, //ln(33/32)
							 32, //ln(65/64)
							 16, //ln(129/128)
							 8, //ln(257/256)
							 4, //ln(513/512)
							 2, //ln(1025/1024)
							 1 //ln(2049/2048) 
};

unsigned short k_table[NUM_TERMS] = 
               {32, 16, 8, 4, 2, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};

#elif PRECISION == 10
unsigned short log_table[NUM_TERMS] = { 4259, //ln(64)
                             3549, //ln(32)
							 2839, //ln(16)
							 2129, //ln(8)
							 1420, //ln(4)
							 710, //ln(2)
							 415, //ln(3/2)
							 228, //ln(5/4)
							 121, //ln(9/8)
							 62, //ln(17/16)
							 32, //ln(33/32)
							 16, //ln(65/64)
							 8, //ln(129/128)
							 4, //ln(257/256)
							 2, //ln(513/512)
							 1 //ln(1025/1024)
};

unsigned short k_table[NUM_TERMS] = 
               { 64, 32, 16, 8, 4, 2, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };

#elif PRECISION == 9
unsigned short log_table[NUM_TERMS] = { 2484, //ln(128)
                             2129, //ln(64)
							 1774, //ln(32)
							 1420, //ln(16)
							 1065, //ln(8)
							 710, //ln(4)
							 355, //ln(2)
							 208, //ln(3/2)
							 114, //ln(5/4)
							 60, //ln(9/8)
							 31, //ln(17/16)
							 16, //ln(33/32)
							 8, //ln(65/64)
							 4, //ln(129/128)
							 2, //ln(257/256)
							 1 //ln(513/512)
};

unsigned short k_table[NUM_TERMS] = 
               {128, 64, 32, 16, 8, 4, 2, 2, 4, 8, 16, 32, 64, 128, 256, 512 };

#endif

#define ATAN_TAB_N 16
unsigned short atantable[ATAN_TAB_N] = {  0x4000,   //atan(2^0) = 45 degrees
                                0x25C8,   //atan(2^-1) = 26.5651
                                0x13F6,   //atan(2^-2) = 14.0362
                                0x0A22,   //7.12502
                                0x0516,   //3.57633
                                0x028B,   //1.78981
                                0x0145,   //0.895174
                                0x00A2,   //0.447614
                                0x0051,   //0.223808
                                0x0029,   //0.111904
                                0x0014,   //0.05595
                                0x000A,   //0.0279765
                                0x0005,   //0.0139882
                                0x0003,   //0.0069941
                                0x0002,   //0.0035013
                                0x0001    //0.0017485
};


// Function definitions

// |var1 - var2|
unsigned short abs_sub(unsigned short var1, unsigned short var2) {

	if(var1 > var2) {
		return var1 - var2;
	} else {
		return var2 - var1;
	}
}

// A basic squaring operation.
unsigned short mypow2(signed short base) {

	unsigned short result;

	// Overflow checking...
	if(base > 255 || base < -255) {
		// Overflow will occur... flag w/ negative output
		result = -1;
	} else {
		// No overflow will occur
		result = base * base;
	}

	return result;
}

// Caculate (var1^2) / (2 * (var2^2))
unsigned short log_exp_term(unsigned short var1, unsigned short var2) {

	unsigned short result;

	if(var2 > var1) {
		result = 0;
	} else {
		result  = mypow2(var1/var2);
		result = result / 2;
	}

	return result;
}

// Calculate log(var) (base e), a very crude approximation.
unsigned short mylog(unsigned short var) {
	int Nis;
	for(Nis = 0;Nis < 16;Nis++) {
		if((var>>Nis) <= 1) {
			break;
		}
	}
	return (69*Nis)/100;
}

// Note: precision is currently 11, or y/2048 for real answer
signed short qlog(unsigned short var)
{
	static unsigned short y = 0;
	static signed short counter = 0;
	static unsigned x = 0;
	x = var;	

	for (counter = 0; counter < (23 * (12 - PRECISION)); counter++)
	{
		if (x >> counter < 1)
			break;
	}

	y = counter * log_table[(15 - PRECISION)];
	counter = PRECISION - counter;
	if (counter > 1)
		x = x * (1 << counter);
	else
	{
		counter = counter * -1;
		x = x / (1 << (counter));
		counter = counter * -1;
	}
	
	for (counter = 0; counter < NUM_TERMS; counter++)
	{
		if (counter < (16 - PRECISION))
		{
			if (k_table[counter] * x <= (1 << PRECISION))
			{
				x = x * k_table[counter];
				y = y - log_table[counter];
			}
		}
		else if (x / k_table[counter] * (k_table[counter] + 1) + x % k_table[counter] 
				 <= (1 << PRECISION))
		{
			x = x / k_table[counter] * (k_table[counter] + 1) + x % k_table[counter];
			y = y - log_table[counter];
		}
	}

	return y;
}

// Calculate the square root of var.
unsigned short mysqrt(unsigned short var) {

	unsigned short y = 1;
	unsigned short counter = 0;
	if (var < 1)
		return 1;
	else if (var > 65535)
		return -1;
	else {
		for (counter = 0; counter < 15; counter++) {
			y = (var/y + y) / 2;
		}
		return y;
	}
}

// Note: qsin/2^15 returns answer
signed short qsin(signed short theta)
{
	signed short sin_result, cos_result;
	cordic_sincos(theta, 16, &sin_result, &cos_result);
	return sin_result;
}

// Note: qcos/2^15 returns answer
signed short qcos(signed short theta)
{
	signed short sin_result, cos_result;
	cordic_sincos(theta, 16, &sin_result, &cos_result);
	return cos_result;
}

// A helper function for qsin and qcos
void cordic_sincos(signed short theta, 
                   char iterations, 
                   signed short *sin_result,
                   signed short *cos_result) 
{
  signed short sigma, s, x1, x2, y, i, quadAdj, shift;
  unsigned short *atanptr = atantable;

  //Limit iterations to number of atan values in our table
  iterations = (iterations > ATAN_TAB_N) ? ATAN_TAB_N : iterations;

  //Shift angle to be in range -180 to 180
  while(theta < -180) theta += 180;
  while(theta > 180) theta -= 180;

  //Shift angle to be in range -90 to 90
  if (theta < -90){
    theta = theta + 180;
    quadAdj = -1;
  } else if (theta > 90){
    theta = theta - 180;
    quadAdj = -1;
  } else{
    quadAdj = 1;
  }

  //Shift angle to be in range -45 to 45
  if (theta < -45){
    theta = theta + 90;
    shift = -1;
  } else if (theta > 45){
    theta = theta - 90;
    shift = 1;
  } else{
    shift = 0;
  }

  //convert angle to decimal representation N = ((2^16)*angle_deg) / 180
  if(theta < 0){
    theta = -theta;
    theta = ((unsigned int)theta<<10)/45;   //Convert to decimal representation of angle
    theta = (unsigned int)theta<<4;
    theta = -theta;
  } else{
    theta = ((unsigned int)theta<<10)/45;   //Convert to decimal representation of angle
    theta = (unsigned int)theta<<4;
  }

  //Initial values
  x1 = 0x4DBA;    //this will be the cosine result, 
                  //initially the magic number 0.60725293
  y = 0;          //y will contain the sine result
  s = 0;          //s will contain the final angle
  sigma = 1;      //direction from target angle
  
  for (i=0; i<iterations; i++){
    sigma = (theta - s) > 0 ? 1 : -1;
    if(sigma < 0){
      x2 = x1 + (y >> i);
      y = y - (x1 >> i);
      x1 = x2;
      s -= *atanptr++;
    } else{
      x2 = x1 - (y >> i);
      y = y + (x1 >> i);
      x1 = x2;
      s += *atanptr++;
    }

  }

  //Correct for possible overflow in cosine result
  if(x1 < 0) x1 = -x1;
  
  //Push final values to appropriate registers
  if(shift > 0){
    *sin_result = x1;
    *cos_result = -y;
  } else if (shift < 0){
    *sin_result = -x1;
    *cos_result = y;
  } else {
    *sin_result = y;
    *cos_result = x1;
  }

  //Adjust for sign change if angle was in quadrant 3 or 4
  *sin_result = quadAdj * *sin_result;
  *cos_result = quadAdj * *cos_result;
}
