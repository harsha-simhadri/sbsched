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


template <class AT, class BT, class F>
void map (AT* A, BT* B, int n, F f) {
  if (n<_SCAN_BSIZE) {
    for (int i=0; i<n; ++i)
      B[i] = f(A[i]);
  } else {
    cilk_spawn map<AT,BT,F> (A,B,n/2,f);
      map<AT,BT,F> (A+n/2,B+n/2,n-n/2,f);
    cilk_sync;
  }
}

template <class E, class F>
void recursiveRepeatedMap (E *A,E *B,int n,F f, int times, double cut_ratio) {
#define _PAR_MAP_THRESOLD_RRM (1<<13)

  if (n > _PAR_MAP_THRESOLD_RRM) 
    for (int i=0; i<times; ++i)
      map<E,E,F> (A,B,n,f);
  else
    return;

  long int cut = (int)(((double)n)*cut_ratio);
  cilk_spawn
    recursiveRepeatedMap<E,F> (A,B,cut,f,times,cut_ratio);
    recursiveRepeatedMap<E,F> (A+cut,B+cut,n-cut,f,times,cut_ratio);
  cilk_sync;  
}

int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,1)) ? 10000000 : get_size(argv, argc,2);
  int repeats = (-1==get_size(argv, argc,2)) ? 3 : get_size(argv, argc,3);
  int cut_ratio = (-1==get_size(argv, argc,3)) ? 50 : get_size(argv, argc,4);

  long long unsigned int alloc_size = 2*LEN*sizeof(double)+10;
  char* space = (char*) newA (char,alloc_size);
  //stripe(space,alloc_size);  

  double* A = (double*)space; double* B=((double*)space)+LEN;
  for (int i=0; i<LEN; ++i) {
    A[i] = i; B[i] = 0;
  }

  // flush_cache(num_procs,sizes[1]);
  std::cout<<"Len: "<<LEN<<" Repeats: "<<repeats<<" Cut_Ratio: "<<cut_ratio<<std::endl
	   <<"Cilk workers: "<<__cilkrts_get_nworkers()<<std::endl;

  initPCM();
  before_sstate = getSystemCounterState(); 
  before_ts = my_timestamp();
  recursiveRepeatedMap<double, plusOne<double> > (A,B,LEN,plusOne<double>(),repeats,0.5);
  after_ts = my_timestamp();
  after_sstate = getSystemCounterState();   
  printDiff();
}
