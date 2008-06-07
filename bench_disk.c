#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <endian.h>
#include <sys/ioctl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <linux/types.h>
#include <linux/hdreg.h>
#include <linux/major.h>
#include <asm/byteorder.h>

#include "bench_disk.h"


extern const char *minor_str[];




#ifndef BLKGETSIZE64
#define BLKGETSIZE64		_IOR(0x12,114,size_t)
#endif

#define TIMING_BUF_MB		2
#define TIMING_BUF_BYTES	(TIMING_BUF_MB * 1024 * 1024)
#define BUFCACHE_FACTOR		2

void flush_buffer_cache (int fd)
{
        fsync (fd);                             /* flush buffers */
        if (ioctl(fd, BLKFLSBUF, NULL))         /* do it again, big time */
                perror("BLKFLSBUF failed");
#ifdef HDIO_DRIVE_CMD
        if (ioctl(fd, HDIO_DRIVE_CMD, NULL) && errno != EINVAL) /* await completion */
                perror("HDIO_DRIVE_CMD(null) (wait for flush complete) failed");
#endif
}


int read_big_block (int fd, char *buf)
{
	int i, rc;
	if ((rc = read(fd, buf, TIMING_BUF_BYTES)) != TIMING_BUF_BYTES) {
		if (rc) {
			if (rc == -1)
				perror("read() failed");
			else
				fprintf(stderr, "read(%u) returned %u bytes\n", TIMING_BUF_BYTES, rc);
		} else {
			fputs ("read() hit EOF - device too small\n", stderr);
		}
		return 1;
	}
	/* access all sectors of buf to ensure the read fully completed */
	for (i = 0; i < TIMING_BUF_BYTES; i += 512)
		buf[i] &= 1;
	return 0;
}

static int do_blkgetsize (int fd, unsigned long long *blksize64)
{
	int		rc;
	unsigned int	blksize32 = 0;

	if (0 == ioctl(fd, BLKGETSIZE64, blksize64)) {	// returns bytes
		*blksize64 /= 512;
		return 0;
	}
	rc = ioctl(fd, BLKGETSIZE, &blksize32);	// returns sectors
	if (rc)
		perror(" BLKGETSIZE failed");
	*blksize64 = blksize32;
	return rc;
}

double bench_disk (char *device) {
	char *buf;
	double elapsed;
	struct itimerval e1, e2;
	int shmid;
	int fd;
	unsigned int max_iterations = 1024, total_MB, iterations;

	fd = open(device, O_RDONLY|O_NONBLOCK);
	if (fd < 0) {
		return 1;
	}

	if ((shmid = shmget(IPC_PRIVATE, TIMING_BUF_BYTES, 0600)) == -1) {
		perror ("could not allocate sharedmem buf");
		return;
	}
	if (shmctl(shmid, SHM_LOCK, NULL) == -1) {
		perror ("could not lock sharedmem buf");
		(void) shmctl(shmid, IPC_RMID, NULL);
		return;
	}
	if ((buf = shmat(shmid, (char *) 0, 0)) == (char *) -1) {
		perror ("could not attach sharedmem buf");
		(void) shmctl(shmid, IPC_RMID, NULL);
		return;
	}
	if (shmctl(shmid, IPC_RMID, NULL) == -1)
		perror ("shmctl(,IPC_RMID,) failed");

	/* Clear out the device request queues & give them time to complete */
	sync();
	sleep(3);

	/*
	 * getitimer() is used rather than gettimeofday() because
	 * it is much more consistent (on my machine, at least).
	 */
	setitimer(ITIMER_REAL, &(struct itimerval){{1000,0},{1000,0}}, NULL);

	/* Now do the timings for real */
	iterations = 0;
	getitimer(ITIMER_REAL, &e1);
	do {
		++iterations;
		if (read_big_block (fd, buf))
			goto quit;
		getitimer(ITIMER_REAL, &e2);
		elapsed = (e1.it_value.tv_sec - e2.it_value.tv_sec)
		 + ((e1.it_value.tv_usec - e2.it_value.tv_usec) / 1000000.0);
	} while (elapsed < 3.0 && iterations < max_iterations);

	total_MB = iterations * TIMING_BUF_MB;

quit:
	if (-1 == shmdt(buf))
		perror ("could not detach sharedmem buf");

	flush_buffer_cache(fd);
	close(fd);

	return total_MB / elapsed;
}

