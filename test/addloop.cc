#include <iostream>
#include "gettime.h"
#include <malloc.h>
//#include <cilk.h>

static int __ii =  mallopt(M_MMAP_MAX,0);
static int __jj =  mallopt(M_TRIM_THRESHOLD,-1);

typedef double E;

int cilk_main () {
  int LEN = (1<<27);
  E *A = new E[LEN];
  E *B = new E[LEN];
  E *C = new E[LEN];

  cilk_for (int i=0; i<LEN; ++i) {
    A[i]=i; B[i]=2*i;
  }

  cilk_for (int i=0; i<LEN; ++i) {
	C[i]=A[i]+B[i];
  }

  startTime ();
  cilk_for (int i=0; i<LEN; ++i) {
	C[i]=A[i]+B[i];
  }
  nextTime ("Time to Add");

  // To make sure compiler doesn't empty out the previous loop 
  E sum=0;
  for (int i=0; i<LEN/56; i+=56) 
    sum += C[i];
  std::cout<<"Sum: "<<sum<<std::endl;
}
