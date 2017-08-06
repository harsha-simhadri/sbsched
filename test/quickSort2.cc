#include <cmath>

#include "ThreadPool.hh"
#include "quickSort2.hh"
#include "parse-args.hh"

typedef double E;

int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? (1<<25) : get_size(argv, argc,2);

  set_proc_affinity (0);

#define blocks 2
  char* space[blocks];
  int alloc_sizes[blocks] = {sizeof(E)*(LEN+1),sizeof(E)*(LEN+1)};

  for (int i=0;i<blocks;++i) {
    space[i] = (char*) newA (char, alloc_sizes[i]);
    stripe(space[i], alloc_sizes[i], 1<<12);
  }
  E *A = (E*)(space[0]);
    E *B = (E*)(space[1]);
  
  for (int i=0; i<LEN; ++i)
    A[i] = rand();

  Scheduler *sched=create_scheduler (argv, argc);
  SizedJob* rootJob = new QuickSort<E, std::less<E> >
    (A,LEN,std::less<E>(), B);
  std::cout<<"Len: "<<LEN<<std::endl;  
  std::cout<<"Root Job Size: "<<rootJob->size(64)/1000000<<"MB"<<std::endl;

  flush_cache(num_procs,sizes[1]);  

  startTime();
  tp_init (num_procs, map, sched, rootJob);
  tp_sync_all ();
  nextTime("QuickSort, measured from driver program");

  std::cout<<"Checking: "<<std::endl;
  if (checkSort (A,LEN,less_equal<E>()))
    std::cout<<"Good"<<std::endl;
  else
    std::cout<<"Bad"<<std::endl;
}
