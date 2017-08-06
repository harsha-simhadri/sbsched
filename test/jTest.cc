
//
// arch-tag: 2dcf5618-b51f-4a04-9f3a-86ab829bc7d8
//

#include <stdlib.h>
#include <cmath>
#include <sched.h>

#include "ThreadPool.hh"
#include "machine-config.hh"
#include "utils.hh"
using namespace std;

//#include "getperf.hh"
#define SPLIT_DEPTH 5
#define SEQ_DEPTH 10
#define REPEATS 20

class jTest : public SizedJob {
  char* A; int n;
  int stride; int offset; int depth; 
  int stage;

public:  
  jTest(char* A_, int n_, bool del=true)
    : SizedJob(del), A(A_), n(n_), stride(1), offset(0), depth(0), stage(0) {}
  
  jTest(char* A_, int n_, int stride_, int offset_, int depth_, int stage_, bool del=true) 
    : SizedJob(del), A(A_), n(n_), stride(stride_), offset(offset_), depth(depth_), stage(stage_) {}

  jTest(jTest* from, bool del=true)
    : SizedJob(del), A(from->A), n(from->n), stride(from->stride), offset(from->offset), depth(from->depth), stage(from->stage+1) {}
    
  lluint size (const int block_size) {
    return (lluint)(n*sizeof(char));
  }

  void function() {
    if (stage >= 1) { 
      join();
    } else {
      if (depth >= SEQ_DEPTH) {
	for (int j = 1; j < REPEATS; j++) {
	  for (int i = offset; i < n; i+=stride) {
	    A[i] += 1;
	  }
	}
	join();
      } else if (depth < SPLIT_DEPTH) {
	binary_fork(new jTest(A,n,2*stride,offset,depth+1,stage),
		    new jTest(A,n,2*stride,offset+stride,depth+1,stage),
		    new jTest(this));
      } else {
	binary_fork(new jTest(A,n/2,stride,offset, depth+1,stage),
		    new jTest(A,n/2,stride,offset+n/2,depth+1,stage),
		    new jTest(this));
      }
    }
  }
};



void flush_cache (int len) {
  char *flush = new char[len]; char sum;
  for (int i=0; i<len; ++i)
    sum += flush[i];
  delete flush;
}

int
main (int argv, char **argc) {

  Scheduler *sched;
  FIND_MACHINE;
  
  if (argv == 3) {
    if (*argc[2] == 'B' || *argc[2]=='b')
      sched = new Scheduler(num_procs);
    else if (*argc[2] == 'W' || *argc[2]=='w')
      sched = new WS_Scheduler(num_procs);
    else if (*argc[2] == 'H' || *argc[2]=='h')
      sched = new HR_Scheduler (num_procs, num_levels, fan_outs, sizes, block_sizes);
    else {
      std::cerr<<"Usage: cmd <size> <W/H>"<<std::endl;
      exit(-1);
    }
  } else {
    std::cerr<<"Usage: cmd <size> <W/H>"<<std::endl;
    exit(-1);
  }

  int LEN;
  if (argv>=2)
    LEN = atoi (argc[1]);
  
  char* A = new char[LEN];
  for (int i=0; i<LEN; ++i) {
    A[i] = rand();
  }
  flush_cache(LEN);
    
  startTime();
  tp_init (num_procs, map, sched,
	   new jTest(A,LEN));
  tp_sync_all ();

  free(A);  
}
