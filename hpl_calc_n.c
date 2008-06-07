#include <stdio.h>
#include <math.h>

/* what percent of ram are we shooting for */
#define MEM_PERCENT .94;
#define MEM_RESERVE 1024 * 512;

int main(int argc, char **argv) {

	long long int ram;
	long int divider;
	long double n;
	long double jn;
	double mem_percent = MEM_PERCENT;

	ram = atoll(argv[1]);

	ram = ram - MEM_RESERVE;

	ram = ram * 1024 * MEM_PERCENT;

	n = (long double) ram / 8;

	n = sqrtl(n);

	divider = (int)n / 128;
	n = divider * 128;

	printf("%.0Lf\n", n);
}
