//
// arch-tag: 2dcf5618-b51f-4a04-9f3a-86ab829bc7d8
//

#include <inttypes.h>           /* for PRIu64 definition */
#include <stdint.h>             /* for uint64_t and PRIu64 */
#include <stdlib.h>
#include <cmath>
#include "libperf.h"
//#include <iostream>

void flush_cache (int len) {
  double *flush = new double[len]; double sum;
  for (int i=0; i<len; ++i)
    sum += flush[i];
  delete flush;
}


int
main (int argv, char **argc) {
  int LEN = 100000000;
  if (argv>=2)
    LEN = atoi (argc[1]);

  double* A = new double[LEN];
  double* B = new double[LEN];
  for (int i=0; i<LEN; ++i) {
    A[i] = i;
    B[i] = 2*i;
  }
  flush_cache(LEN);
  /*
  struct perf_data* pd = libperf_initialize (-1,-1);
  libperf_enablecounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES);
  libperf_enablecounter (pd,LIBPERF_COUNT_HW_CACHE_LL_STORES_MISSES);
  libperf_enablecounter (pd,LIBPERF_COUNT_HW_CACHE_L1D_LOADS_MISSES);
  libperf_enablecounter (pd,LIBPERF_COUNT_HW_CACHE_L1D_STORES_MISSES);

  for (int i=0; i<LEN; ++i)
    B[i] = 1+A[i];

  libperf_finalize (pd,NULL);

  fprintf(stdout, "LL_LM: %" PRIu64 "\n", libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES));
  fprintf(stdout, "LL_LM: %" PRIu64 "\n", libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_STORES_MISSES));
*/
  
  int perf_global_LL_LM = new_counter (LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES,  -1, -1);
  int perf_global_LL_SM = new_counter (LIBPERF_COUNT_HW_CACHE_LL_STORES_MISSES, -1, -1);
  int perf_global_L1D_LM = new_counter (LIBPERF_COUNT_HW_CACHE_L1D_LOADS_MISSES,  -1, -1);
  int perf_global_L1D_SM = new_counter (LIBPERF_COUNT_HW_CACHE_L1D_STORES_MISSES, -1, -1);
  
  enable_counter (perf_global_LL_LM);
  enable_counter (perf_global_LL_SM);
  enable_counter (perf_global_L1D_LM);
  enable_counter (perf_global_L1D_SM);

  for (int i=0; i<LEN; ++i)
    B[i] = 1+A[i];

  std::cout<<"Global LL_LM: "<<read_counter(perf_global_LL_LM)/1000000.0<<"M"<<std::endl;
  std::cout<<"Global LL_SM: "<<read_counter(perf_global_LL_SM)/1000000.0<<"M"<<std::endl;
  std::cout<<"Global L1D_LM: "<<read_counter(perf_global_L1D_LM)/1000000.0<<"M"<<std::endl;
  std::cout<<"Global L1D_SM: "<<read_counter(perf_global_L1D_SM)/1000000.0<<"M"<<std::endl;
 
}

