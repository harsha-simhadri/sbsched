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

// -*- C++ -*-

#include <stdlib.h>
#include <cmath>

#include "ThreadPool.hh"
#include "machine-config.hh"
#include "intSort.hh"
#include "utils.hh"
//#include "getperf.hh"

using namespace intSort;
using namespace  utils;

#define CHECK 1
#define TTYPE double

template <class T>
void check(bool cond, int i, T val, string s) {
  if (!cond) { 
    cout << "Error for " << s << ": at i=" << i << " v=" << val << endl; 
    abort(); } 
}

template <class T>
void check2(bool cond, int i, int j, T val, string s) {
  if (!cond) { 
    cout << "Error for " << s << ": at i=" << i << " j=" << j << 
      " v=" << val << endl; 
    abort(); } 
}

template <class E>
struct mod {
  int mval;
  mod(int bits) : mval((1<<bits)-1) {}
  int operator() (E i) {return ((int) i) & mval;}
};

int main(int argc, char* argv[]) {
  FIND_MACHINE;

  int n = 10;
  if (argc > 1) n = std::atoi(argv[1]);
  long t1;

  {
    int mult = 4;

    // make sure mem is allocated
    TTYPE *i1 = newA(TTYPE,mult*n);
    for(int i =0; i < mult*n; i++) i1[i] = 0;

    TTYPE *i2 = newA(TTYPE,mult*n);
    for(int i =0; i < mult*n; i++) i2[i] = 0;

    free(i1); free(i2);
  }

  typedef unsigned int uint;

  {
    uint *A = newA(uint,n);
    for (int i = 0; i < n; i++) { 
      A[i] = hash(i+n);
    }
    long maxVal = ((long) 1) << 32;

    iSort<uint, identityF<uint> > * root = 
      new iSort<uint, identityF<uint> >(A,NULL, n,maxVal,true,identityF<uint>());
   
    Scheduler *sched;

    if (argc == 3)
      if (*argv[2] == 'W' || *argv[2]=='w')
	sched = new WS_Scheduler(num_procs);
      else if (*argv[2] == 'H' || *argv[2]=='h')
	sched = new HR_Scheduler (num_procs, num_levels, fan_outs, sizes, block_sizes);
      else {
	std::cerr<<"Usage: cmd <size> <W/H>"<<std::endl;
	exit(-1);
      }

    //startCounter (LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES , -1);
    startTime();
    tp_init(num_procs,map,sched, root);
    
    tp_sync_all();
    //std::cout<<"LL misses: "<<readCounter()<<std::endl;
    nextTime("Integer Sort (32 bit, random)");

    if (CHECK)
      for (int i=1; i < n; i++) 
	check((A[i-1]<=A[i]),i,A[i],"radix sort (int)");
    free(A);
  }

  /*
  {
    uint *A = dataGen::expDist<uint>(0,n);
    long maxVal = ((long) 1) << 32;
    startTime();
    intSort::iSort(A,n,maxVal,utils::identityF<uint>());
    stopTime(.1,"Integer Sort (32 bit, exponential)");
    if (CHECK)
      for (int i=1; i < n; i++) 
	check((A[i-1]<=A[i]),i,A[i],"radix sort (int)");
    free(A);
  }

  {
    uint *A = dataGen::rand<uint>(0,n);
    long maxVal = 256;
    startTime();
    intSort::iSort(A,n,maxVal,utils::identityF<uint>());
    stopTime(.1,"Integer Sort (8 bit key, 32 bit data, random)");
    free(A);
  }

  {
    pair<uint,int> *A = new pair<uint,int>[n];
    long maxVal = ((long) 1) << 32;
    cilk_for (int i=0;i<n;i++)  {
      A[i].first = dataGen::hash<uint>(i);
      A[i].second = 0;}
    startTime();
    intSort::iSort(A,n,maxVal,utils::firstF<int,int>());
    stopTime(.2,"Integer Sort (32 bit key, 64 bit data, random)");
    if (CHECK)
      for (int i=1; i < n; i++) 
	check((A[i-1].first<=A[i].first),i,A[i].first,"radix sort (int,int)");
    free(A);
  }
  reportTime("Integer Sort (weighted average)");
  */
}

