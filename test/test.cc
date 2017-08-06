//
// arch-tag: 2dcf5618-b51f-4a04-9f3a-86ab829bc7d8
//

#include <stdlib.h>
#include <cmath>

#include "ThreadPool.hh"
#include "machine-config.hh"
#include "sequence-jobs.hh"
#include "parse-args.hh"

typedef double E;

int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? (1<<25) : get_size(argv, argc,2);
  
  FIND_MACHINE;
  Scheduler *sched=create_scheduler(argv,argc);

  E* A = (E*) calloc(sizeof(E),LEN);
  E* B = (E*) calloc(sizeof(E),LEN);
  srand(1);
  for(int i=0; i<LEN; i++) {
      A[i] = rand();
      B[i] = rand();
  }

  SizedJob *root_job = new Empty<E, E, Id<E> >(A,B,LEN,Id<E>());
  
  std::cout<<"Len: "<<LEN<<std::endl;
  std::cout<<"Job size: "<<root_job->size(64)/1000000.<<"MB"<<std::endl;

  flush_cache(4,sizes[1]);
  startTime();
  tp_init (num_procs, map, sched, root_job);
  tp_sync_all ();
  nextTime ("Time from driver program");

  E sumA=0, sumB=0;
  for (int i=0; i<LEN; ++i) {
    sumA += A[i]; sumB += B[i];
  }
  if (sumA!=sumB)
    std::cerr<<"Checksum failed"<<std::endl;
	   
  
}
