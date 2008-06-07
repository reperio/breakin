
/***************************************************************************
 *   modified from the standard stream (not the  test, but the output)
 ***************************************************************************/

/*-----------------------------------------------------------------------*/
/* Program: Stream                                                       */
/* Revision: $Id: bench_stream.c,v 1.2 2006/04/10 05:14:08 ksheumaker Exp $ */
/* Original code developed by John D. McCalpin                           */
/* Programmers: John D. McCalpin                                         */
/*              Joe R. Zagar                                             */
/*                                                                       */
/* This program measures memory transfer rates in MB/s for simple        */
/* computational kernels coded in C.                                     */
/*-----------------------------------------------------------------------*/
/* Copyright 1991-2005: John D. McCalpin                                 */
/*-----------------------------------------------------------------------*/
/* License:                                                              */
/*  1. You are free to use this program and/or to redistribute           */
/*     this program.                                                     */
/*  2. You are free to modify this program for your own use,             */
/*     including commercial use, subject to the publication              */
/*     restrictions in item 3.                                           */
/*  3. You are free to publish results obtained from running this        */
/*     program, or from works that you derive from this program,         */
/*     with the following limitations:                                   */
/*     3a. In order to be referred to as "STREAM benchmark results",     */
/*         published results must be in conformance to the STREAM        */
/*         Run Rules, (briefly reviewed below) published at              */
/*         http://www.cs.virginia.edu/stream/ref.html                    */
/*         and incorporated herein by reference.                         */
/*         As the copyright holder, John McCalpin retains the            */
/*         right to determine conformity with the Run Rules.             */
/*     3b. Results based on modified source code or on runs not in       */
/*         accordance with the STREAM Run Rules must be clearly          */
/*         labelled whenever they are published.  Examples of            */
/*         proper labelling include:                                     */
/*         "tuned STREAM benchmark results"                              */
/*         "based on a variant of the STREAM benchmark code"             */
/*         Other comparable, clear and reasonable labelling is           */
/*         acceptable.                                                   */
/*     3c. Submission of results to the STREAM benchmark web site        */
/*         is encouraged, but not required.                              */
/*  4. Use of this program or creation of derived works based on this    */
/*     program constitutes acceptance of these licensing restrictions.   */
/*  5. Absolutely no warranty is expressed or implied.                   */
/*-----------------------------------------------------------------------*/
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <sys/time.h>

#include "bench_stream.h"

int bench_stream(struct bench_stream_t *data) {
    int			quantum, checktick();
    int			BytesPerWord;
    register int	j, k;
    double		scalar, t, times[4][STREAM_NTIMES];

	static double	a[STREAM_N+STREAM_OFFSET],
		b[STREAM_N+STREAM_OFFSET],
		c[STREAM_N+STREAM_OFFSET];

	static double	avgtime[4] = {0}, maxtime[4] = {0},
		mintime[4] = {FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX};

	static char	*label[4] = {"STREAM_COPY", "STREAM_SCALE",
    		"STREAM_ADD", "STREAM_TRIAD"};

	static double	bytes[4] = {
    		2 * sizeof(double) * STREAM_N,
    		2 * sizeof(double) * STREAM_N,
    		3 * sizeof(double) * STREAM_N,
    		3 * sizeof(double) * STREAM_N
    	};


    /* --- SETUP --- determine precision and check timing --- */

    	BytesPerWord = sizeof(double);

    /* Get initial value for system clock. */
    for (j=0; j<STREAM_N; j++) {
	a[j] = 1.0;
	b[j] = 2.0;
	c[j] = 0.0;
    }

    t = mysecond();
    for (j = 0; j < STREAM_N; j++)
	a[j] = 2.0E0 * a[j];
    t = 1.0E6 * (mysecond() - t);

    scalar = 3.0;
    for (k=0; k<STREAM_NTIMES; k++) {
	times[0][k] = mysecond();
#pragma omp parallel for
	for (j=0; j<STREAM_N; j++)
	    c[j] = a[j];
	times[0][k] = mysecond() - times[0][k];
	
	times[1][k] = mysecond();
#pragma omp parallel for
	for (j=0; j<STREAM_N; j++)
	    b[j] = scalar*c[j];
	times[1][k] = mysecond() - times[1][k];
	
	times[2][k] = mysecond();
#pragma omp parallel for
	for (j=0; j<STREAM_N; j++)
	    c[j] = a[j]+b[j];
	times[2][k] = mysecond() - times[2][k];
	
	times[3][k] = mysecond();
#pragma omp parallel for
	for (j=0; j<STREAM_N; j++)
	    a[j] = b[j]+scalar*c[j];
	times[3][k] = mysecond() - times[3][k];
	}

    /*	--- SUMMARY --- */

    for (k=1; k<STREAM_NTIMES; k++) /* note -- skip first iteration */
	{
	for (j=0; j<4; j++)
	    {
	    avgtime[j] = avgtime[j] + times[j][k];
	    mintime[j] = STREAM_MIN(mintime[j], times[j][k]);
	    maxtime[j] = STREAM_MAX(maxtime[j], times[j][k]);
	    }
	}

    data->copy = 1.0E-06 * bytes[0]/mintime[0];
    data->scale = 1.0E-06 * bytes[1]/mintime[1];
    data->add = 1.0E-06 * bytes[2]/mintime[2];
    data->triad = 1.0E-06 * bytes[3]/mintime[3];
    
    return 0;
}

# define	STREAM_M	20

int checktick() {
    int		i, minDelta, Delta;
    double	t1, t2, timesfound[STREAM_M];

/*  Collect a sequence of M unique time values from the system. */

    for (i = 0; i < STREAM_M; i++) {
	t1 = mysecond();
	while( ((t2=mysecond()) - t1) < 1.0E-6 )
	    ;
	timesfound[i] = t1 = t2;
	}

    minDelta = 1000000;
    for (i = 1; i < STREAM_M; i++) {
	Delta = (int)( 1.0E6 * (timesfound[i]-timesfound[i-1]));
	minDelta = STREAM_MIN(minDelta, STREAM_MAX(Delta,0));
	}

   return(minDelta);
}

double mysecond() {
        struct timeval tp;
        struct timezone tzp;
        int i;

        i = gettimeofday(&tp,&tzp);
        return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}

