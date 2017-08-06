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

template <class AT, class BT, class F>
class RepeatedMap : public HR2Job {

  AT* A; BT* B; int n; F f;
  int times;
  int stage;

public:

  RepeatedMap (AT *A_, BT *B_, int n_, F f_, int times_, int stage_=0,
       bool del=true)
    : HR2Job (del),
      A(A_), B(B_), n(n_), f(f_), times(times_), stage(stage_)
    {}

  lluint size (const int block_size) {
    return round_up(n*sizeof(AT), block_size)
      + round_up(n*sizeof(BT), block_size);
  }
  
  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size(block_size);
    } else {
      return STRAND_SIZE;
    }
  }
  
  void function () {
    if (stage < times) {
	unary_fork (new Map<AT,BT,F> (A,B,n,f),
		    new RepeatedMap<AT,BT,F> (A,B,n,f,times,stage+1));
    } else if (stage == times) {
      join ();
    } else {
      std::cerr<<"Invalid Stage"<<std::endl;
      exit(-1);
    }
  }
};

template <class E, class F>
class RecursiveRepeatedMap : public HR2Job {
  E *A, *B;
  F f;
  int n, times, stage;
  double cut_ratio;

#define _PAR_MAP_THRESOLD_RRM (1<<13)
  
public:
  RecursiveRepeatedMap (E *A_, E *B_, int n_, F f_, int times_, double cut_ratio_, int stage_=0, bool del=true)
    : HR2Job (del),
      A(A_), B(B_), n(n_), f(f_), times(times_), cut_ratio(cut_ratio_), stage(stage_)
    {}

  lluint size (const int block_size) {return 2*round_up(n*sizeof(E), block_size);}
  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size(block_size);
    } else {
      return STRAND_SIZE;
    }
  }

  void function () {
    if (stage == 0) {
      if (n > _PAR_MAP_THRESOLD_RRM) {
	unary_fork ( new RepeatedMap<E,E,F> (A,B,n,f,times),
		     new RecursiveRepeatedMap<E,F> (A,B,n,f,times,cut_ratio,1) );
      } else {
	join ();
      }
    } else if (stage == 1) {
      long int cut = (int)(((double)n)*cut_ratio);
      binary_fork (new RecursiveRepeatedMap<E,F> (A,B,cut,f,times,cut_ratio,0),
		   new RecursiveRepeatedMap<E,F> (A+cut,B+cut,n-cut,f,times,cut_ratio,0),
		   new RecursiveRepeatedMap<E,F> (A,B,n,f,times,cut_ratio,2) );
      
    } else if (stage == 2) {
      join();
    } else {
      std::cerr<<"Invalid stage: "<<stage <<" in RecursiveScan"<<std::endl;
      exit(-1);
    }
  }
};

int
main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? 100000000 : get_size(argv, argc,2);
  int repeats = (-1==get_size(argv, argc,3)) ? 3 : get_size(argv, argc,3);
  int cut_ratio = (-1==get_size(argv, argc,4)) ? 50 : get_size(argv, argc,4);

  lluint alloc_size = 2*LEN*sizeof(double)+10;
  //int hugepage_id = alloc_hugetlb (alloc_size);
  //char* space = (char*)translate_hugetlb (hugepage_id);
  char *space = newA(char,alloc_size);

  //stripe(space,alloc_size);  
 
  double* A = (double*)space; double* B=((double*)space)+LEN;
  for (int i=0; i<LEN; ++i) {
    A[i] = i; B[i] = 0;
  }

  flush_cache(num_procs,sizes[1]);
  std::cout<<"Len: "<<LEN<<" Repeats: "<<repeats<<" Cut_Ratio: "<<cut_ratio<<std::endl;
 
  Scheduler *sched=create_scheduler (argv, argc);
  startTime();
  tp_init (num_procs, map, sched,
	   new RecursiveRepeatedMap<double, plusOne<double> >(A, B, LEN, plusOne<double>(),repeats, ((double)cut_ratio)/100.0,0.5));

  tp_sync_all ();
  nextTime("Total time, measured from driver program");
  
  //free_hugetlb(hugepage_id);
  /*  double check = 0.0;
  for(int i=0; i<LEN; i++) {
     check += A[i] + B[i];
  }
  printf("Checksum = %lf\n", check); */
}


template <class E, class F>
class nWayRecursiveRepeatedMap : public SizedJob {
  E *A, *B;
  F f;
  int n;
  int stage;

#define _PAR_MAP_THRESOLD (1<<12)
#define NUM_REPEAT 5
  
public:
  nWayRecursiveRepeatedMap (E *A_, E *B_, int n_, F f_, int stage_=0, bool del=true)
    : SizedJob (del),
      A(A_), B(B_), n(n_), f(f_), stage(stage_)
    {}

  lluint size (const int block_size) {return 2*round_up(n*sizeof(E), block_size);}

  void function () {
    if (stage == 0) {
      if (n > _PAR_MAP_THRESOLD) {
	unary_fork ( new RepeatedMap<E,E,F> (A,B,n,f,NUM_REPEAT),
		     new RecursiveRepeatedMap<E,F> (A,B,n,f,1) );
      } else {
	join ();
      }
    } else if (stage == 1) {
      #define NWAY 4
      int segLen = n/NWAY;
      Job **children = new Job*[NWAY];
      for (int i=0; i<NWAY; ++i)
	children[i] = new nWayRecursiveRepeatedMap<E,F> (A+i*segLen,B+i*segLen,segLen,f,0);
      fork (NWAY,
	    children,
	    new nWayRecursiveRepeatedMap<E,F> (A,B,n,f,2) );
      
    } else if (stage == 2) {
      join();
    } else {
      std::cerr<<"Invalid stage: "<<stage <<" in RecursiveScan"<<std::endl;
      exit(-1);
    }
  }
 };
