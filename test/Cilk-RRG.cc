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


//#include "parse-args.hh"
#include <cstdlib>
#include <iostream>
#include "common.hh"
#include "recursion_basecase.hh"
#include "utils.hh"
#include "test.h"


#include <cilk/cilk.h>
#include <cilk/cilk_api.h>

long long int 
get_size (int argv, char **argc, int pos=2) {
  if (argv>pos)
    return atoi (argc[pos]);
  else 
    return -1;
}


template <class E>
void gather (E* A, E* B, int* hash, int n) {
  if (n<_SCAN_BSIZE) {
    for (int i=0; i<n; ++i)
      B[i] = A[hash[i]%n];
  } else {
    cilk_spawn gather<E> (A,B,hash,n/2);
    gather<E> (A+n/2,B+n/2,hash+n/2,n-n/2);
    cilk_sync;
  }
}

template <class E>
void recursiveRepeatedGather (E *A,E *B,int* hash,int n, int times, double cut_ratio) {
#define _PAR_MAP_THRESOLD_RRM (1<<13)

  if (n > _PAR_MAP_THRESOLD_RRM) 
    for (int i=0; i<times; ++i)
      gather<E> (A,B,hash,n);
  else
    return;

  long int cut = (int)(((double)n)*cut_ratio);
  cilk_spawn  recursiveRepeatedGather<E> (A,B,hash,cut,times,cut_ratio);
  recursiveRepeatedGather<E> (A+cut,B+cut,hash+cut,n-cut,times,cut_ratio);
  cilk_sync;
  
}


int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? 10000000 : get_size(argv, argc,2);
  int repeats = (-1==get_size(argv, argc,3)) ? 3 : get_size(argv, argc,3);
  int cut_ratio = (-1==get_size(argv, argc,4)) ? 50 : get_size(argv, argc,4);

  long long unsigned int alloc_size = 2*LEN*sizeof(double)+10+LEN*sizeof(int);
  char* space = (char*) newA (char,alloc_size);
  //stripe(space,alloc_size);  
 
  double* A = (double*)space; double* B=((double*)space)+LEN;
  int* hash= (int*)(((double*)space)+2*LEN);
  for (int i=0; i<LEN; ++i) {
    A[i] = i; B[i] = 0;  hash[i]=utils::hash(i)%LEN;
  }

  // flush_cache(num_procs,sizes[1]);
  std::cout<<"Len: "<<LEN<<" Repeats: "<<repeats<<" Cut_Ratio: "<<cut_ratio<<std::endl
	   <<"Cilk workers: "<<__cilkrts_get_nworkers()<<std::endl;

  initPCM();
  before_sstate = getSystemCounterState(); 
  before_ts = my_timestamp();
  recursiveRepeatedGather<double> (A,B,hash,LEN,repeats,0.5);
  after_ts = my_timestamp();
  after_sstate = getSystemCounterState();   
  printDiff();
}
