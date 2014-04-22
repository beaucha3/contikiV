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
							 1, //ln(2049/2048) 
};

unsigned short k_table[NUM_TERMS] = {32, 16, 8, 4, 2, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048};

#elif PRECISION == 10
int log_table[NUM_TERMS] = { 4259, //ln(64)
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
							 1, //ln(1025/1024)
};

int k_table[NUM_TERMS] = { 64, 32, 16, 8, 4, 2, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024 };

#elif PRECISION == 9
int log_table[NUM_TERMS] = { 2484, //ln(128)
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
							 1, //ln(513/512)
};

int k_table[NUM_TERMS] = {128, 64, 32, 16, 8, 4, 2, 2, 4, 8, 16, 32, 64, 128, 256, 512 };

#endif


int main()
{
	unsigned short x = 0;
	unsigned short y = 0;
	unsigned short counter = 0;
	printf("Value for x? ");
	scanf("%hd", &x);
	

	for (counter = 0; counter < (23 * (12 - PRECISION)); counter++)
	{
		if (x >> counter < 1)
			break;
	}

	y = counter * log_table[(15 - PRECISION)];
	x = x * (1 << PRECISION) / (1 << counter);

	for (counter = 0; counter < NUM_TERMS; counter++)
	{
		if (counter < (16 - PRECISION))
		{
			while (k_table[counter] * x <= (1 << PRECISION))
			{
				x = x * k_table[counter];
				y = y - log_table[counter];
			}
		}
		else if (x * (k_table[counter] + 1) / k_table[counter] <= (1 << PRECISION))
		{
			x = x * (k_table[counter] + 1) / k_table[counter];
			y = y - log_table[counter];
		}
	}

	printf("ln(x) in Q%d = %hd\nln(x) by integer approximation = %hd\n", PRECISION, y, y/(1 << PRECISION));
}
