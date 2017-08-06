//
// arch-tag: 2dcf5618-b51f-4a04-9f3a-86ab829bc7d8
//

#include <stdlib.h>
#include "ThreadPool.hh"
#include "machine-config.hh"
#include "sequence-jobs.hh"
#include "parse-args.hh"

int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? 100000000 : get_size(argv, argc,2);
  
  double* A = new double[LEN];
  double* B = new double[LEN];
  double** A_ptr = new double*[LEN];
  double** B_ptr = new double*[LEN];
  for (int i=0; i<LEN; ++i) {
    A[i] = i; A_ptr[i] = A+i;
    B[i] = 0; B_ptr[i] = B+i;
  }

  FIND_MACHINE;
  flush_cache(num_procs,sizes[1]);
  Scheduler *sched=create_scheduler (argv, argc);
  
  startTime();
  tp_init (num_procs, map, sched,
           new RecursiveGatherScatter<double>(A_ptr, B_ptr, LEN, 5));
  tp_sync_all ();
  nextTime("Total time, measured from driver program");
  
  /*  double check = 0.0;
  for(int i=0; i<LEN; i++) {
     check += *A_ptr[i] + *B_ptr[i];
  }
  printf("Checksum = %lf\n", check); */
}
