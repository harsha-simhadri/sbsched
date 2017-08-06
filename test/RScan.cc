//
// arch-tag: 2dcf5618-b51f-4a04-9f3a-86ab829bc7d8
//

#include <stdlib.h>
#include "ThreadPool.hh"
#include "machine-config.hh"
#include "sequence-jobs.hh"
#include "parse-args.hh"


template <class E, class F>
class RecursiveScan : public HR2Job {
  E *A; E *B;
  F f;
  int n;
  int stage;
  E zero;

#define _PAR_SCAN_THRESOLD (1<<12)
  
public:
  RecursiveScan (E *A_, E *B_, int n_, F f_, E zero_, int stage_=0, bool del=true)
    : HR2Job (del),
      A(A_), B(B_), n(n_), f(f_), zero(zero_), stage(stage_)
    {}

  lluint size (const int block_size) {return 2*round_up(n*sizeof(E), block_size);}
  lluint strand_size (const int block_size) {return size(block_size);}

  void function () {
    if (stage == 0) {
      if (n > _PAR_SCAN_THRESOLD) {
	unary_fork ( new Scan<E,F > (new E, A,B,n,f,zero),
		     new RecursiveScan<E,F> (A,B,n,f,zero,1) );
      } else {
	join ();
      }
    } else if (stage == 1) {
      binary_fork (new RecursiveScan<E,F> (A,B,n/2,f,zero,0),
		   new RecursiveScan<E,F> (A+n/2,B+n/2,n-n/2,f,zero,0),
		   new RecursiveScan<E,F> (A,B,n,f,zero,2) );
      
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

  double* A = new double[LEN];
  double* B = new double[LEN];

  for (int i=0; i<LEN; ++i) {
    A[i] = 1;
    B[i] = 0;
  }
  double sum;

  FIND_MACHINE;
  flush_cache(num_procs,sizes[1]);
  Scheduler *sched=create_scheduler (argv, argc);
  
  std::cout<<"Len: "<<LEN<<std::endl;
  startTime();
  tp_init (num_procs, map, sched,
	   //new Scan<double,plus<double> > (&sum, A,A,LEN,plus<double>(),0.0));
	   new RecursiveScan<double,plus<double> > (A,B,LEN,plus<double>(),0.0));


  tp_sync_all ();
  nextTime("Total time, measured from driver program");
  
  // std::cout<<"Checksum: "<<B[LEN-1]<<" Last element: "<<sum<<std::endl;
}

template <class E, class F>
class RepeatedScan : public SizedJob {
  E *A, *B, *C;
  F f;
  int n; int sum;
  int stage; int level;

public:
  RepeatedScan (E *A_, E *B_, E *C_, int n_, F f_, int level_, int stage_, bool del=true)
    : SizedJob (del),
      A(A_), B(B_), C(C_), n(n_), f(f_), stage(stage_), level(level_)
    {}

  lluint size (const int block_size) {return 3*round_up(n*sizeof(E), block_size);}

  void function () {
    // std::cout<<A<<" "<<B<<" "<<n<<" "<<stage<<std::endl;
    if (stage == 0) {
      if (level == 2) {
	for (int i=0; i<n;++i)
	  sum = f(A[i],B[i]);
	join();
      } else if (level == 1) {
	#define NUM_REPEAT_LEVEL_ONE 5
        #define NUM_PARTITIONS_LEVEL_ONE 4
	Job **children = new Job*[NUM_REPEAT_LEVEL_ONE*NUM_PARTITIONS_LEVEL_ONE];
	for (int i=0; i<NUM_REPEAT_LEVEL_ONE; ++i)
	  for (int j=0; j<NUM_PARTITIONS_LEVEL_ONE; ++j)
	    children[i*NUM_PARTITIONS_LEVEL_ONE+j]
	      = new RepeatedScan<E,F> (A+j*(n/NUM_PARTITIONS_LEVEL_ONE),
				       B+j*(n/NUM_PARTITIONS_LEVEL_ONE),
				       C+j*(n/NUM_PARTITIONS_LEVEL_ONE),
				       n/NUM_PARTITIONS_LEVEL_ONE, f, 2,0);

	fork (NUM_REPEAT_LEVEL_ONE*NUM_PARTITIONS_LEVEL_ONE,
	      children, new RepeatedScan<E,F> (A,B,C,n,f,1,1) );
      } else {
        #define FAN_OUT 500
	Job **children = new Job*[FAN_OUT];
	for (int i=0; i<FAN_OUT; ++i)
	  children[i] = new RepeatedScan<E,F> (A+i*(n/FAN_OUT), B+i*(n/FAN_OUT), C+i*(n/FAN_OUT),
					       n/FAN_OUT, f, 1,0);

	fork (FAN_OUT,
	      children, new RepeatedScan<E,F> (A,B,C,n,f,0,1) );
      }
    } else if (stage == 1) {
      join();
    } else {
      std::cerr<<"Invalid stage: "<<stage <<" in RecursiveScan"<<std::endl;
      exit(-1);
    }
  }
};

/* The task is cut in to n-different parts and B=f(A) applied to each */
template <class E, class F>
class DistinctScans : public SizedJob {
  E *A, *B;
  F f;
  int n;
  int procs;
  int stage;

public:
  DistinctScans (E *A_, E *B_, int n_, F f_, int procs_=1, int stage_=0, bool del=true)
    : SizedJob (del),
      A(A_), B(B_), n(n_), f(f_), procs(procs_), stage(stage_)
    {}

  lluint size (const int block_size) {return 2*round_up(n*sizeof(E), block_size);}

  void function () {
    if (stage == 0) {
      Job **children = new Job*[procs];
      int segSize = n/procs;
      for (int i=0; i<procs; ++i)
	children[i] = new DistinctScans<E,F>(A+i*segSize, B+i*segSize, segSize, f, 1, 1);
      fork (procs, children, new DistinctScans<E,F>(A,B,n,f,procs,2));
    } else if (stage == 1) {
	for (int i=0;i<n;++i)
	  B[i] = f(A[i]);
	join();
    } else {
      join();
    }
  }
};
