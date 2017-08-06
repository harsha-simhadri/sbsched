#include <stdlib.h>
#include <pthread.h>
#include "gettime.h"
#include <iostream>
using namespace std;

#define NTHREADS    31
#define ARRAYSIZE   20000000
#define ITERATIONS   ARRAYSIZE / NTHREADS

double *A;
double *B;
long times[NTHREADS];
long proc[NTHREADS];

void do_work(int pid)
{
  int i, start, end;
  long ts = example_get_time();

  start = (pid * ITERATIONS);
  end = start + ITERATIONS;
  for (i=start; i < end ; i++) {
    A[i] = B[i];
  }
  times[pid] = example_get_time() - ts;
  proc[pid] = sched_getcpu();
}


int main(int argc, char *argv[])
{
  A = (double*) malloc(sizeof(double)*ARRAYSIZE);
  B = (double*) malloc(sizeof(double)*ARRAYSIZE);
  cilk_for (int i=0; i<ARRAYSIZE; ++i) {
    A[i] = i;
    B[i] = 0;
  }

  long ts = example_get_time();
  _Pragma("cilk grainsize = 1")
  cilk_for (int i=0; i<NTHREADS; i++) do_work(i);
  //cilk_for (int i=0; i<ARRAYSIZE; i++) A[i] = B[i];

  cout << "Total time: " << example_get_time() - ts << endl;

  for (int i=0; i < NTHREADS; i++) {
    cout << i << " : " << times[i] << ", " << proc[i] << endl;
  }

  free(A); free(B);
}

