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


#ifndef __SCHEDULER_HH
#define __SCHEDULER_HH

#include "Job.hh"
#include "syncQueue.hh"
#include <deque>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>
#include <sys/types.h>
#include <linux/unistd.h>
#include <unistd.h>
#include <stdint.h>
#include <assert.h>

class Scheduler {
protected:
  int               _num_threads;                 // Number of threads (also num procs??)
  std::vector<Job*> _job_queue;                   // Jobs to be done
  Mutex             _queue_lock;
  pid_t *           _pthread_map;                  // Pthread ID map
public:
  Scheduler (int num_threads)
    : _num_threads (num_threads)//, _pool(NULL)
    {}
  bool check_range  (int id, int start, int end,
		     std::string * func_name);    // Check if the thread id is in proper range (start inclusive, end exclusive), print error and exit otherwise
  void set_pthread_map (pid_t *map) {_pthread_map=map;}
  
  virtual void add  (Job *job, int thread_id);    // Add a job to the task queue, -1 thread_id for anon enqueues
  virtual void add_multiple (int num_jobs, Job **jobs, int thread_id);
  virtual void done (Job *job, int thread_id,     // This job is done, -1 thread_id for anon calls
		     bool deactivate) {}          // Is this the end of the task of which the strand is a part
  virtual Job* get  (int thread_id=-1);           // Get a job. if more(x) returned TRUE, calling this immediately should return a job
  virtual bool more (int thread_id=-1);           // if arg=-1, check if any jobs in system,
                                                  // else, check if any jobs that can be handled by this thread
                                                  // implementations of derived classes should confirm to this
  virtual void print_scheduler_stats();
};


class Local_Scheduler : public Scheduler {
protected:
  int                 _num_jobs;                  // Total number of jobs 
  std::vector<Job*> * _job_queues;                // One queue per processor
public:
  Local_Scheduler (int num_thr)
    : Scheduler (num_thr),
      _num_jobs (0) {
    _job_queues = new std::vector<Job*>[_num_threads];
  }
  ~Local_Scheduler () {
    delete _job_queues;
  }
  
  void add  (Job *job, int thread_id );           // Add a job to the task queue, -1 thread_id for anon enqueues
  void add_multiple (int num_jobs, Job **jobs, int thread_id);
  Job* get  (int thread_id=-1);                   // Get a job             
  bool more (int thread_id=-1);                
  void done (Job *job, int thread_id,
		     bool deactivate);
  void print_scheduler_stats() {};
};

  
#endif
