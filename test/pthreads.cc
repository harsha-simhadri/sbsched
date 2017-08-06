#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <iostream>
using namespace std;

#define NTHREADS    31
#define ARRAYSIZE   20000000
#define ITERATIONS   ARRAYSIZE / NTHREADS

int example_get_time()
{
    static timeval* start = NULL;
    struct timezone tzp = {
     0, 0
    };
    if (NULL == start) {
        // define the current time as 0.
        start = (timeval *) malloc(sizeof(timeval));
        gettimeofday(start, &tzp);
        return 0;
    } else {
        // subtract the start time from the current time.
        timeval end;
        long ms = 0;
        gettimeofday(&end, &tzp);
        ms = (end.tv_sec - start->tv_sec) * 1000;
        ms += ((int) end.tv_usec - (int) start->tv_usec) / 1000;
        return (int) ms;
    }
}

double *A;
double *B;
long times[NTHREADS];
long proc[NTHREADS];

void *do_work(void *tid) 
{
  int i, start, *mytid, end;
  long ts = example_get_time();

  mytid = (int *) tid;
  start = (*mytid * ITERATIONS);
  end = start + ITERATIONS;
  for (i=start; i < end ; i++) {
    A[i] = B[i];
  }
  times[*mytid] = example_get_time() - ts;
  proc[*mytid] = sched_getcpu();
  pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
  int i, start, tids[NTHREADS];
  pthread_t threads[NTHREADS];
  pthread_attr_t attr;

  A = (double*) malloc(sizeof(double)*ARRAYSIZE);
  B = (double*) malloc(sizeof(double)*ARRAYSIZE);
  _Pragma("omp parallel for")
  for (int i=0; i<ARRAYSIZE; ++i) {
    A[i] = i;
    B[i] = 0;
  }

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  long ts = example_get_time();
  for (i=0; i<NTHREADS; i++) {
    tids[i] = i;
    pthread_create(&threads[i], &attr, do_work, (void *) &tids[i]);
  }

  for (i=0; i<NTHREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  cout << "Total time: " << example_get_time() - ts << endl;

  for (int i=0; i < NTHREADS; i++) {
    cout << i << " : " << times[i] << ", " << proc[i] << endl;
  }

  free(A); free(B);
  pthread_attr_destroy(&attr);
  pthread_exit (NULL);
}

