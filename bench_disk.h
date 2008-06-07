#ifndef _BENCH_DISK_H_
#define _BENCH_DISK_H_

void flush_buffer_cache (int fd);
int read_big_block (int fd, char *buf);
static int do_blkgetsize (int fd, unsigned long long *blksize64);
double bench_disk (char *device);

#endif
