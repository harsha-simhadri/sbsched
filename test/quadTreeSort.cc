// This code is part of the project "Experimental Analysis of Space-Bounded
// Schedulers", presented at Symposium on Parallelism in Algorithms and
// Architectures, 2014.
// Copyright (c) 2014 Harsha Vardhan Simhadri, Guy Blelloch, Phillip Gibbons,
// Jeremy Fineman, Aapo Kyrola.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <stdlib.h>
#include <cmath>

#include "ThreadPool.hh"
#include "machine-config.hh"
#include "quadTreeSort.hh"
#include "parse-args.hh"

//Leave this as a power of 2 if you want the test code to be correct.
#define MAX_COORD (1<<15)

//Test code only works for integers since I wanted to do the simple
//bit shifts to implement it.
typedef int E;

int main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? (1<<25) : get_size(argv, argc,2);

  set_proc_affinity (0);

  char* space[7];
  lluint alloc_sizes[7] = {sizeof(Point<E>)*(LEN+1),sizeof(int)*(LEN+1),sizeof(int)*(LEN+1),
		     sizeof(int)*(LEN+1),sizeof(int)*(LEN+1),sizeof(int)*(LEN+1),
		     sizeof(Point<E>)*(LEN+1)};
  
  FIND_MACHINE;  
  //int hupepage_id = alloc_hugetlb (quadSortSize<E>(LEN,block_sizes[0]));
  //char* space = (char*)translate_hugetlb (hupepage_id);
  for(int i=0;i<7;++i) {
    space[i] = (char*) newA (char, alloc_sizes[i]+block_sizes[0]);
    if (space[i]==NULL) std::cerr<<"NewA failed"<<std::endl;
    stripe(space[i],alloc_sizes[i]);
  }

  Point<E>* A = (Point<E>*) (space[0]);
  int *compared = (int*)(space[1]);
  int *tl_pos = (int*)(space[2]);
  int *tr_pos = (int*)(space[3]);
  int *bl_pos = (int*)(space[4]);
  int *br_pos = (int*)(space[5]);
  Point<E> *B = (Point<E>*)(space[6]);

  Box<E> bx(0,MAX_COORD);
  for (int i=0; i<LEN; ++i) {
    A[i].x = (rand()%MAX_COORD); 
    A[i].y = (rand()%MAX_COORD);
  }

  std::cout<< "the bounding box: (" << bx.tl.x << "," << bx.tl.y << ")  (" << bx.br.x << "," << bx.br.y << ")\n";
  
  Scheduler *sched=create_scheduler (argv, argc);
  SizedJob* rootJob = new QuadTreeSort<E> (A,bx,LEN,
					     compared, tl_pos, tr_pos, bl_pos, br_pos, B);
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

  //free_hugetlb(hupepage_id);
}
