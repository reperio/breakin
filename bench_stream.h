#ifndef _BENCH_STREAM_H_
#define _BENCH_STREAM_H_

#define STREAM_N	5000000 
#define STREAM_NTIMES	10
#define STREAM_OFFSET	0

# ifndef STREAM_MIN
# define STREAM_MIN(x,y) ((x)<(y)?(x):(y))
# endif
# ifndef STREAM_MAX
# define STREAM_MAX(x,y) ((x)>(y)?(x):(y))
# endif

struct bench_stream_t {
	double	copy;
	double	scale;
	double	add;
	double	triad;
};

double mysecond();
int bench_stream(struct bench_stream_t *data);


#endif
