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


#ifndef __THREADPOOL_HH
#define __THREADPOOL_HH
//
//  Project : ThreadPool
//  File    : ThreadPool.hh
//  Author  : Ronald Kriemann
//  Purpose : class for managing a pool of threads
//
// arch-tag: 7b363e2f-5623-471f-b0dd-55d7ac55b93e
//

#include <inttypes.h>
#include <stdint.h>
#include "Thread.hh"
#include "Job.hh"
#include "Scheduler.hh"
#include "WSScheduler.hh"
#include "HR1Scheduler.hh"
#include "HR2Scheduler.hh"
#include "HR3Scheduler.hh"
#include "HR4Scheduler.hh"

#include "gettime.hh"
//#include "libperf.h"

// no specific processor
#define NO_PROC ((uint) -1)

class ThreadPool;

// Thread handled by threadpool
class PoolThr : public Thread {
  friend class ThreadPool;
protected:
  ThreadPool       * _pool;           // pool we are in
  
  Job              * _job;            // job to run and data for it

  bool               _done;           // Thread has come out of infinite loop
  bool               _end;            // indicates end-of-thread
  Mutex              _del_mutex;      // mutex for preventing premature deletion
    
public:
  PoolThr ( const int n, ThreadPool * p )
    : Thread(n), _pool(p),
      _job(NULL), _end(false),
      _done(false)
    {}
  ~PoolThr () {}//;

  ThreadPool*  get_pool ();
  virtual void inf_loop ();            // parallel running method
  void         run_job  ();            // Run the job, unlock, and delete if needed
  void         add_job  (Job* job);    // Add job to threadpool's scheduler' taskQ
  void         quit     ();            // quit thread (reset data and wake up)
};

// takes jobs and executes them in threads
class ThreadPool {
  friend class PoolThr;
  
protected:
  uint              _max_parallel;     // maximum degree of parallelism
  PoolThr  **       _threads;          // array of threads, handled by pool
  pid_t *           _pthread_map;      // array to store pthread_ids of threads
public:
  uint              _idle_count;       // number of idle threads
  bool              _null_join;        // Has a fork with null continuation been called?
  Condition         _idle_cond;        // condition for synchronisation of idle list

  Scheduler *       _scheduler;        // Task order handler
  Condition         _scheduler_cond;   // Mutex/Cond to access job queue
  Condition         _inf_loop_cond;    // Signal to start inf  loop in decentral threadpool
  Mutex             _print_lock;
public:
  ThreadPool ( const uint max_p,
	       Scheduler * sched = NULL,
	       uint * proc_ids=NULL);  // Set the threads's affinity to proc_ids
  ~ThreadPool ();
  int   set_thread_affinity (
                      uint thread_id,  // Set the affinity of thread_id to proc_id
		      uint proc_id);   // Return -1 on fail, 0 if not
  
  uint  max_parallel () const { return _max_parallel; }

  void  add_job  ( Job * job,          // Add job, if spawned at 'fork', it is @thr
		   PoolThr * thr=NULL);// If its a cont_job, last job 'join'ing was @thr
  void  add_jobs ( int num_jobs,
		   Job ** jobs,
		   PoolThr * thr=NULL);
  void  add_job  ( Job * job,          // Calls add_job (Job*, PoolThr*) using the thread_id
		   uint thread_id);    // to find the thread pointer
  void  done_job ( Job * job,          // Done with this job.
		   PoolThr* thr,       // If its a cont_job, last job 'join'ing was @thr
		   bool deactivate);   // false, if this is called after 'fork', true if after 'join'
  void  sync     ( Job * job );        // Wait till job is done    
  void  sync_all ();                   // Wait till all threads return
  void  change_idle_count (int diff);  // change _idle_cont atomically
  bool  null_joined () {               // Check on status of null join
        return _null_join; }
  void  set_null_join () {
        _null_join = false;}
  void  reset_null_join () {
        _null_join = true;}  
};

// To access the global thread-pool

void start_timers (uint num_procs);
void print_timers (uint num_procs);

void tp_init ( const uint p,
	       uint * proc_ids = NULL, // Thread-processor affinities
	       Scheduler * sched=NULL, // init global thread_pool
	       Job * root = NULL);     // If job is specified, tp_run is implicitly called here
void tp_run  ( Job * job );            // run job
void tp_sync ( Job * job );            // synchronise with specific job
void tp_sync_all ();                   // synchronise with all jobs
void tp_done ();                       // finish thread pool, implicitly syncs--waits for all jobs to be done and all threads to become idle

#endif  // __THREADPOOL_HH
