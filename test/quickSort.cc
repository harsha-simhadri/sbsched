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
#include "quickSort.hh"
#include "parse-args.hh"

typedef double E;

int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? (1<<25) : get_size(argv, argc,2);

  set_proc_affinity (0);

  int hugepage_id[5];
  char* space[5];
  int alloc_sizes[5] = {sizeof(E)*(LEN+1),sizeof(int)*(LEN+1),
			sizeof(int)*(LEN+1),sizeof(int)*(LEN+1),sizeof(E)*(LEN+1)};

  for (int i=0;i<5;++i) {
    //hugepage_id[i] = alloc_hugetlb (alloc_sizes[i]);
    //space[i] = (char*)translate_hugetlb (hugepage_id[i]);
    space[i] = (char*) newA (char, alloc_sizes[i]);
    stripe(space[i], alloc_sizes[i]);
  }
  E *A = (E*)(space[0]);
  int *compared = (int*)(space[1]);
  int *less_pos = (int*)(space[2]);
  int *more_pos = (int*)(space[3]);
  E *B = (E*)(space[4]);
  
  for (int i=0; i<LEN; ++i)
    A[i] = rand();

  Scheduler *sched=create_scheduler (argv, argc);
  SizedJob* rootJob = new QuickSort<E, std::less<E> >
    (A,LEN,std::less<E>(),
     compared, less_pos, more_pos, B);
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

  for (int i=0; i<5; ++i) {
         free_hugetlb(hugepage_id[i]);  
  }
}
