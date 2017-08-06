
//
// arch-tag: 2dcf5618-b51f-4a04-9f3a-86ab829bc7d8
//

#include <stdlib.h>
#include <cmath>

#include "ThreadPool.hh"
#include "machine-config.hh"

class AddContJob : public SizedJob {
  double  *_sum, *_sum1, *_sum2;
  lluint _size;
public:
  AddContJob (double *sum, double *sum1, double *sum2, lluint size, bool del=true)
    : SizedJob (del),
      _sum (sum),
      _sum1 (sum1),
      _sum2 (sum2),
      _size (size)
  {}
  
  void
  function () {
    //std::cout<<__func__<<std::endl;
    *_sum = *_sum1 + *_sum2;
    //std::cout<<*_sum<<std::endl;
    join();
  }

  lluint size (const int block_size) {return _size;}
};

class AddJob : public SizedJob {
  double * _A;
  int   _len;
  double * _sum;
  lluint _size;
public:
  AddJob (double *array, int len, lluint size, bool del=true)
    : SizedJob (del),
      _A (array),
      _len (len),
      _size (size)
    {_sum = new double;}
  lluint size (const int block_size) {return _size;}
  
  void
  function () {
    if (_len <= (1<<18)) {
      double sum=0;
      int length = _len;
      for (int i=0; i<length; ++i)
	sum += _A[i];
      *_sum=sum;
      join();
    } else {
      Job ** jobs = new Job*[2];
      jobs[0] = new AddJob (_A, _len/2, (lluint)(1.1*(_len/2)));
      jobs[1] = new AddJob (_A+_len/2, _len-_len/2, (lluint)(1.1*(_len-_len/2)));
      AddContJob *sumUp = new AddContJob (_sum,
                                          static_cast<AddJob*>(jobs[0])->_sum,
                                          static_cast<AddJob*>(jobs[1])->_sum,
                                          _size);
      fork (2, jobs, sumUp);
    }
  }
};

void add (int num_threads) {
#define LEN (1<<30)
  double *A = new double[LEN];
  for (int i=0; i<LEN; ++i)
    A[i] = i;
  AddJob * root = new AddJob (A, LEN, (lluint)(1.1*LEN), false);

  FIND_MACHINE;
  startTime();
/*  tp_init (num_procs, map,
	   new HR_Scheduler (num_procs,
			     num_levels, fan_outs, sizes, block_sizes),
			     root);*/
 tp_init (num_procs, NULL, new WS_Scheduler(num_procs), root);
  tp_sync_all ();
  nextTime("Done adding");
  tp_run (root);
  tp_sync_all();  
  nextTime("Done adding twice");
}

int
main (int argv, char **argc) {
  int num_threads=8;
  if (argv >= 2)
    num_threads = atoi (argc[1]);
    
  add(num_threads);
}
