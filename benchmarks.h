#ifndef _BENCHMARKS_H_
#define _BENCHMARKS_H_

#include "bench_stream.h"
#include "bench_disk.h"

struct benchmarks_t {
	int			completed;
	struct bench_stream_t	stream;
} benchmarks;

#endif
