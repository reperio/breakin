#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* what percent of ram are we shooting for */
#define MEM_PERCENT .94;
#define MEM_RESERVE 1024 * 768;

int main(int argc, char **argv) {

	unsigned long long int ram;
	long int divider;
	long double n;
	long double jn;
	double mem_percent = MEM_PERCENT;

	ram = strtoull(argv[1], (char **)NULL, 10);


	ram = ram - MEM_RESERVE;

	ram = ram * 1024 * MEM_PERCENT;


	n = (long double) ram / 8;

	n = sqrtl(n);


	divider = (long int)n / 192;
	n = divider * 192;

	printf("%.0Lf\n", n);
}
