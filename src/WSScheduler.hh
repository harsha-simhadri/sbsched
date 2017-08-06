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


#ifndef __WSSCHEDULER_HH
#define __WSSCHEDULER_HH

#include "Scheduler.hh"

class WS_Scheduler : public Scheduler {
protected:
  int                 _num_jobs;                  // Total number of jobs
  lluint            * _num_steals;                // Number of steals, one counter for each job
  std::vector<Job*> * _job_queues;                // One queue per processor
  Mutex             * _local_lock;                // Local processor locks this before grabbing a locally queued job
  Mutex             * _steal_lock;                // Stealing procs grab this lock before locking the local lock
public:
  WS_Scheduler (int num_threads)
    : Scheduler (num_threads),
      _num_jobs (0) {
    _job_queues = new std::vector<Job*>[_num_threads];
    _local_lock = new Mutex[num_threads];
    _steal_lock = new Mutex[num_threads];
    _num_steals = new lluint[num_threads];
  }
  ~WS_Scheduler();
  int steal_choice (int thread_id);               // Which queue to steal from, when you run out of work
  
  void add  (Job *job, int thread_id );           // Add a job to the task queue, -1 thread_id for anon enqueues
  void add_multiple  (int num_jobs,Job **jobs, int thread_id );
  Job* get  (int thread_id=-1);                   // Get a job             
  bool more (int thread_id=-1);                
  void done (Job *job, int thread_id,
		     bool deactivate);
  void print_scheduler_stats();
};

class PWS_Scheduler : public WS_Scheduler {
protected:
  int _fan_out; int _cluster_size; double _steal_ratio; 
  double _large, _small;
public:
  PWS_Scheduler (int num_threads, int fan_out, double steal_ratio)  // Assume we care about first level fan out
    : WS_Scheduler (num_threads), _fan_out(fan_out), _steal_ratio(steal_ratio) {

    _cluster_size = _num_threads/_fan_out;
    assert (_fan_out * _cluster_size == _num_threads);

    _large = _steal_ratio/(_steal_ratio*_cluster_size + (_num_threads-_cluster_size));
    _small = 1.0/(_steal_ratio*_cluster_size + (_num_threads-_cluster_size));
  }

  int steal_choice (int thread_id);
  Job* get  (int thread_id=-1);                   // Get a job             
};

#endif
