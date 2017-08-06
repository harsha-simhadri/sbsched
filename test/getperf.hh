#ifndef __GETPERF_HH
#define __GETPERF_HH

#include <inttypes.h>           /* for PRIu64 definition */
#include <stdint.h>             /* for uint64_t and PRIu64 */
#include <stdio.h>              /* for printf family */
#include <stdlib.h>             /* for EXIT_SUCCESS definition */
#include "libperf.h"            /* standard libperf include */

static perf_data *pd ;
static int _counter;

inline void startCounter (int counter, int cpu) {
  pd = libperf_initialize(-1, cpu);
  _counter=counter;
  libperf_enablecounter(pd, _counter);
}

inline uint64_t readCounter () {
  return  libperf_readcounter(pd, _counter);
}

void disableCounter () {
  libperf_disablecounter(pd, _counter);
}

#endif
