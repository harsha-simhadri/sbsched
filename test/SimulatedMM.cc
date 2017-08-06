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


//
// arch-tag: 2dcf5618-b51f-4a04-9f3a-86ab829bc7d8
//

#include <stdlib.h>
#include "ThreadPool.hh"
#include "machine-config.hh"
#include "sequence-jobs.hh"
#include "parse-args.hh"


template <class ET>
class SimulatedMM : public HR2Job {

  ET* A; ET *B; // Row major layout, pointer point to starting point in the matrix
  int A_row_size, B_row_size;
  int n,m,p; double cut_ratio;
  ET *sum; ET **child_sum;
  int n_way;int stage;

public:

  SimulatedMM (ET* A_, ET* B_, int A_row_size_, int B_row_size_, 
	       ET* sum_, ET** child_sum_,
	       int n_, int m_, int p_, double cut_ratio_,
	       int n_way_, int stage_=0, bool del=true)
    : HR2Job (del),
      A(A_), B(B_), 
      A_row_size(A_row_size_), B_row_size(B_row_size_),
      sum(sum_), child_sum(child_sum_),
      n(n_), m(m_), p(p_), cut_ratio(cut_ratio_), n_way(n_way_),
      stage(stage_)
  {}

#define SimulatedMM_PAR_THRESHOLD (1<<12)

  lluint size (const int block_size) {
    lluint sizeoftask=round_up((n*m+m*p)*sizeof(ET), block_size);
    assert (sizeoftask>0);
    return sizeoftask;
  }

  lluint strand_size (int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size(block_size);
    } else {
      
      return (n*m+m*p<SimulatedMM_PAR_THRESHOLD)
	? size(block_size) : STRAND_SIZE ;
    }

  }

  inline int offsetA (int i, int j) {
    return i*A_row_size + j;
  }

  inline int offsetB (int i, int j) {
    return i*B_row_size + j;
  }

  void function () {
    if (stage == 0) {
      if (n*m + m*p < SimulatedMM_PAR_THRESHOLD) {
	for (int i=0; i<n; ++i)
	  for (int j=0; j<p; ++j)
	    for (int k=0; k<m; ++k)
	      *sum += A[offsetA(i,k)] * B[offsetB(k,j)];
	join ();
      } else {
	child_sum = new ET*[n_way];
	Job **children = new Job*[n_way];

	int child_size_sum=0; 
	for (int i=0; i<n_way; ++i) {
	  child_sum[i] = new ET;
	  int x,y,z,n_cut,m_cut,p_cut;
	  x=i&0x001; y=(i&0x2)>>1; z=(i&0x4)>>2;
	  n_cut = n*cut_ratio; m_cut = m*cut_ratio; p_cut = p*cut_ratio;
	  children[i] = new SimulatedMM<ET> (A+offsetA(x*n_cut,y*m_cut),B+offsetB(y*m_cut,z*p_cut),
					     A_row_size, B_row_size,
					     child_sum[i],NULL,
					     x*(n-2*n_cut)+n_cut,y*(m-2*m_cut)+m_cut,z*(p-2*p_cut)+p_cut,cut_ratio,
					     n_way,0);
	  //child_size_sum += (y*(m-2*m_cut)+m_cut) * (x*(n-2*n_cut)+n_cut + z*(p-2*p_cut)+p_cut); 
	}
	//assert (child_size_sum == 2*m*(n+p)) 
	fork (n_way,
	      children,
	      new SimulatedMM<ET> (A,B,A_row_size,B_row_size,sum,child_sum,n,m,p,cut_ratio,n_way,1));
      }
    } else if (stage == 1) {
      for (int i=0; i<n_way; ++i)
	*sum+=*child_sum[i];
      join();
    }
  }
};

int
main (int argv, char **argc) {
  int LEN =  get_size(argv, argc, 2);
  int cut_percentage = get_size(argv, argc, 3);

  LEN = (LEN==-1) ? (1<<12) : LEN;
  cut_percentage = (cut_percentage==-1) ? 50 : cut_percentage;

  int n=LEN,m=LEN,p=LEN;
  double cut_ratio=((double)cut_percentage)/100;
  std::cout<<"len: "<<LEN<<";  cut_percent: "<<cut_percentage<<std::endl;

  double* A = new double[n*m];
  double* B = new double[m*p];
  double sum;

  for (int i=0; i<n*m; ++i) 
    A[i] = i; 
  for (int i=0; i<m*p; ++i) 
    B[i] = 2*i; 

  FIND_MACHINE;
  flush_cache(num_procs,sizes[1]);

  Scheduler *sched=create_scheduler (argv, argc);
  
  startTime();
  tp_init (num_procs, map, sched,
	   new SimulatedMM<double>(A,B,n,m,&sum,NULL,n,m,p,cut_ratio,8));

  tp_sync_all ();
  nextTime("Total_T_from_driver: ");
  
  /*  double check = 0.0;
  for(int i=0; i<LEN; i++) {
     check += *A_ptr[i] + *B_ptr[i];
  }
  printf("Checksum = %lf\n", check); */
}
