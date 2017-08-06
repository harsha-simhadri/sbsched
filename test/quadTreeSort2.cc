//
// arch-tag: 2dcf5618-b51f-4a04-9f3a-86ab829bc7d8
//

#include <stdlib.h>
#include <cmath>

#include "ThreadPool.hh"
#include "machine-config.hh"
#include "quadTreeSort2.hh"
#include "parse-args.hh"

//Leave this as a power of 2 if you want the test code to be correct.
#define MAX_COORD (1<<15)

//Test code only works for integers since I wanted to do the simple
//bit shifts to implement it.
typedef int E;

int main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? (1<<25) : get_size(argv, argc,2);

  set_proc_affinity (0);

  #define blocks 3
  int hugepage_id[blocks];
  char* space[blocks];
  lluint alloc_sizes[blocks] = {sizeof(Point<E>)*(LEN+1), 
				sizeof(Point<E>)*(LEN+1),
				sizeof(btype)*(LEN+1)};

  for(int i=0;i<blocks;++i) {
    space[i] = (char*) newA (char, alloc_sizes[i]);
    stripe(space[i], alloc_sizes[i]);  
  }
  
  Point<E>* A = (Point<E>*) (space[0]);
  Point<E> *B = (Point<E>*)(space[1]);
  btype *bucketId = (btype*)(space[2]);

  Box<E> bx(0,MAX_COORD);
  for (int i=0; i<LEN; ++i) {
    A[i].x = (rand()%MAX_COORD); 
    A[i].y = (rand()%MAX_COORD);
    bucketId[i] = 0;
  }

  std::cout<< "the bounding box: (" << bx.tl.x << "," << bx.tl.y << ")  (" << bx.br.x << "," << bx.br.y << ")\n";
  
  Scheduler *sched=create_scheduler (argv, argc);
  SizedJob* rootJob = new QuadTreeSort<E> (A, bx, LEN, B, bucketId);
  std::cout<<"Len: "<<LEN<<std::endl;  
  std::cout<<"Root Job Size: "<<rootJob->size(64)/1000000<<"MB"<<std::endl;

  flush_cache(num_procs,sizes[1]);  

  startTime();
  tp_init (num_procs, map, sched, rootJob);
  tp_sync_all ();
  nextTime("QuadTreeSort, measured from driver program");
  /* 
   * Check only works for integers
   */
  std::cout<<"Checking: "<<std::endl;
  if (checkSort(A,LEN))
    std::cout<<"Good"<<std::endl;
  else
    std::cout<<"Bad"<<std::endl;  
}
