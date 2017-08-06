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


#include <cmath>

#include "ThreadPool.hh"
#include "sequence-jobs.hh"
#include "transpose.hh"
#include "sampleSort.hh"
#include "string.h"
//#include "utils.hh"
#include "parse-args.hh"
//#include "getperf.hh"

template <class T>
class PtrCmpLess {
public:
  inline bool operator() (const T& x, const T& y) const {return *x<*y;}
};

template <class T>
class PtrCmpLessEq {
public:
  inline bool operator() (const T& x, const T& y) const {return *x<=*y;}
};

template <class E>
lluint allocSize (int n, double exp) {
  lluint size =0 ;
  int sq = (int)(pow(n,exp));
  int rowSize = sq*AVG_SEG_SIZE;
  int numR = (int)ceil(((double)n)/((double)rowSize));
  int numSegs = (sq-1)/PIVOT_QUOT;

  int  sampleSetSize = numSegs*OVERSAMPLE;
  size += sizeof(E)*sampleSetSize;
  size += sizeof(E)*(numSegs-1);
  size += sizeof(E)*(numR*rowSize);
  size += 3*sizeof(int)*(numR*numSegs+1);
  return size;
}

#define E double

int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? (1<<25) : get_size(argv, argc,2);
  double exp = (double) ((-1==get_size(argv, argc,3)) ? 50 : get_size(argv, argc,3));
  exp/=100.0;

  Scheduler *sched=create_scheduler(argv, argc);
  
  E** A_ptr = new E*[LEN];
  E* A = new E[LEN];
  stripe((char*)A_ptr,sizeof(E*)*LEN);
  stripe((char*)A,sizeof(E*)*LEN);
  for (int i=0; i<LEN; ++i) {
    A[i] = rand();
    A_ptr[i] = A+hash(i,LEN);
  }

  char *space = newA(char, allocSize<E>(LEN,exp));
  stripe(space,allocSize<E>(LEN,exp));

  SizedJob* root_job = new SampleSort<E*, PtrCmpLess<E*> >(A_ptr,LEN,PtrCmpLess<E*>(),exp,NULL);//space);
  std::cout<<"Len: "<<LEN<<" exp: "<<exp<<std::endl;
  std::cout<<"Size: "<<root_job->size(64)/1000000.<<"MB"<<std::endl;  
  flush_cache(num_procs,sizes[1]);

  startTime();
  tp_init (num_procs, map, sched, root_job);
  tp_sync_all ();
  nextTime ("SampleSort, from the driver program");

if(false) {
  if (checkSort (A_ptr,LEN,PtrCmpLessEq<E*>()))
    std::cout<<"Good"<<std::endl;
  else
    std::cout<<"Bad"<<std::endl;
}
  free(A_ptr);
  free(A);
  
}
