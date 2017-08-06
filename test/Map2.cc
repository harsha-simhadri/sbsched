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

class MapAddOne : public HR2Job {
  double* A; double* B; int n; int stage;

public:
  MapAddOne (double *A_, double *B_, int n_, int stage_=0,
       bool del=true)
    : HR2Job (del),
      A(A_), B(B_), n(n_),  stage(stage_)  {}

  lluint size (const int block_size) { return 0;}
  lluint strand_size (const int block_size) { return 0;}
  
  void function () {
    if (stage == 0) {
      if (n<50000) {
	double x = 0.0;
	for (int i=0; i<n; ++i)
	  B[i] = A[i];
	join ();
      } else {
	binary_fork (new MapAddOne(A,B,n/2),
		     new MapAddOne(A+n/2,B+n/2,n-n/2),
		     new MapAddOne(A,B,n,1));
      }
    } else {
      join ();
    }
  }
};

long times[100];
long proc[100];

class MapGeneric : public HR2Job {
  double* A; double* B; int s; int n; int stage;

public:
  MapGeneric (double *A_, double *B_, int s_, int n_, int stage_=0,
       bool del=true)
    : HR2Job (del),
      A(A_), B(B_), n(n_),  s(s_), stage(stage_)  {}

  lluint size (const int block_size) { return 0;}
  lluint strand_size (const int block_size) { return 0;}
  
  void function () {
    if (stage == 0) {
      if (n==1) {
	long ts = example_get_time();
	double x = 0.0;
	int b = s*20000000/32;
	for (int i=0; i<20000000/32; ++i)
	  B[b+i] = A[b+i];
	times[s] = example_get_time() - ts;
	proc[s] = sched_getcpu();
	join ();
      } else {
	binary_fork (new MapGeneric(A,B,s,n/2),
		     new MapGeneric(A,B,s+n/2,n-n/2),
		     new MapGeneric(A,B,s,n,1));
      }
    } else {
      join ();
    }
  }
};

class MapScatter : public HR2Job {
  double* A; double* B; int* I; int n; int stage;

public:
  MapScatter (double *A_, double *B_, int *I_, int n_, int stage_=0,
       bool del=true)
    : HR2Job (del),
      A(A_), B(B_), I(I_), n(n_),  stage(stage_)  {}

  lluint size (const int block_size) { return 0;}
  lluint strand_size (const int block_size) { return 0;}
  
  void function () {
    if (stage == 0) {
      if (n<50000) {
	for (int i=0; i<n; ++i)
	  B[i] = A[I[i]];
	join ();
      } else {
	binary_fork (new MapScatter(A,B,I,n/2),
		     new MapScatter(A+n/2,B,I+n/2,n-n/2),
		     new MapScatter(A,B,I,n,1));
      }
    } else {
      join ();
    }
  }
};

int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? 100000000 : get_size(argv, argc,2);
  int repeats = (-1==get_size(argv, argc,3)) ? 3 : get_size(argv, argc,3);
  int cut_ratio = (-1==get_size(argv, argc,4)) ? 50 : get_size(argv, argc,4);

  double* A = new double[LEN];
  double* B = new double[LEN];
  int* I = new int[LEN];
  for (int i=0; i<LEN; ++i) {
    A[i] = i;
    B[i] = 0;
    I[i] = rand()%LEN;

  }

  FIND_MACHINE;
  flush_cache(num_procs,sizes[1]);
  Scheduler *sched=create_scheduler (argv, argc);
  
  std::cout<<"Len: "<<LEN<<" Repeats: "<<repeats<<" Cut_Ratio: "<<cut_ratio<<std::endl;
  startTime();
  tp_init (num_procs, map, sched,
	   //	   new RecursiveRepeatedMap<double, plusOne<double> >(A, B, LEN, plusOne<double>(),repeats, ((double)cut_ratio)/100.0,0.5));
	   new MapGeneric(A, B, 0, 32));
  tp_sync_all ();
  nextTime("Total time, measured from driver program");

  for (int i=0; i < 32; i++) {
    cout << i << " : " << times[i] << ", " << proc[i] << endl;
  }
  /*  double check = 0.0;
  for(int i=0; i<LEN; i++) {
     check += A[i] + B[i];
  }
  printf("Checksum = %lf\n", check); */
}



