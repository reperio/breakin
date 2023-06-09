#include <stdio.h>
#include <math.h>
#include <stdlib.h>

int main(int argc, char **argv) {

	/* get the number of cores as first argument */
	int cores = atoi(argv[1]);
	int p = 0;
	int q = 0;

	for (p = ceil(sqrt(cores)); p > 1; --p) {
		if (cores % p == 0) {
			break;
		}
	}
	q = cores / p;

	/* always make Q bigger than P */
	if (p > q) {
		printf("%d            Ps\n", q);
		printf("%d            Qs\n", p);
	}
	else {
		printf("%d            Ps\n", p);
		printf("%d            Qs\n", q);
	}
}
