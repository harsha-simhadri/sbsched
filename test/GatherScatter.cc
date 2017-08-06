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
#include "ThreadPool.hh"
#include "machine-config.hh"
#include "sequence-jobs.hh"
#include "parse-args.hh"

int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? (1<<27) : get_size(argv, argc,2);
  
  double* A = new double[LEN];
  double* B = new double[LEN];
  double** A_ptr = new double*[LEN];
  double** B_ptr = new double*[LEN];
  for (int i=0; i<LEN; ++i) {
    A[i] = i; A_ptr[i] = A+i;
    B[i] = 0; B_ptr[i] = B+i;
  }

  FIND_MACHINE;
  Scheduler *sched=create_scheduler (argv, argc);

  flush_cache(num_procs,sizes[1]);

  std::cout<<"Len: "<<LEN<<std::endl;    
  startTime();
  tp_init (num_procs, map, sched,
	   new GatherScatter<double>(A_ptr, B_ptr, LEN));

  tp_sync_all ();
  nextTime("Total time, measured from driver program");
  
  /*  double check = 0.0;
  for(int i=0; i<LEN; i++) {
     check += *A_ptr[i] + *B_ptr[i];
  }
  printf("Checksum = %lf\n", check); */
}
