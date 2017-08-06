#include "loops.h"


int 
set_proc_affinity (int proc_id) {
  int status;
  pthread_t thread = pthread_self();
  if ( proc_id != -1 ) {
    cpu_set_t affinity;
    CPU_ZERO ( &affinity );
    CPU_SET ( proc_id, &affinity );
    if ((status = pthread_setaffinity_np ( thread, sizeof(affinity), &affinity))
	!= 0) {
      std::cerr << "Setting thread affinity to "<<proc_id<<" failed with error value "
	//		<< strerror ( status ) <<std::endl;
		<<std::endl;
    } 
  }
  
  return status;
}

void runtest(std::string name, testFunction f, int n) {
  libperf_enablecounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS);
  libperf_enablecounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES);
  
  uint64_t LLD_LL_init = libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS);
  uint64_t LLD_LM_init = libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES);
  
  
    f(n);
    
  uint64_t LLD_LL = libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS) - LLD_LL_init;
  uint64_t LLD_LM = libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES) - LLD_LM_init;

  libperf_disablecounter(pd, LIBPERF_COUNT_HW_CACHE_LL_LOADS);                 /* disable HW counter */
  libperf_disablecounter(pd, LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES);                 /* disable HW counter */
  
  std::cout << n << " " << name << ", LLD_LL: " << LLD_LL << std::endl;
  std::cout << n << " " << name << ", LLD_LM: " << LLD_LM << std::endl;
  if (LLD_LL>0)
    std::cout << "Misses/loads: " << (LLD_LM*1.0/LLD_LL) << std::endl;
  
}
  
int
main (int argv, char **argc) {
  int LEN = 100000000;
  if (argv>=2)
    LEN = atoi (argc[1]);

  A = new uint64_t[LEN];
   B = new uint64_t[LEN];
  std::cout << "Length is " << LEN << std::endl;

  
  flush_cache(LEN);

  pd = libperf_initialize (-1,-1);
               /* disable HW counter */
               
  runtest("init", initloop, LEN);               
  runtest("copyloop", copyloop, LEN);          
  runtest("copyloopptr", copyloopptr, LEN);               

/*
  runtest("sumloop", sumloop, LEN);
  runtest("sumloopptr", sumloopptr, LEN);
  runtest("sumloopptr_unroll8", sumloopptr_unroll8, LEN);

  runtest("transformloop", transformloop, LEN);               
  runtest("setA", setA, LEN);               

  runtest("jumpysumloop", jumpysumloop, LEN);               
  runtest("nothing", nothing, LEN);               
  runtest("almostnothing", almostnothing, LEN);               
*/
  libperf_finalize (pd,NULL);

}

