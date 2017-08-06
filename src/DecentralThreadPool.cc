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
//  Project : ThreadPool
//  File    : TThreadPool.cc
//  Author  : Ronald Kriemann
//  Purpose : class for managing a pool of threads
//
// arch-tag: d4739323-4e22-42d9-9fcb-f98f5890d77b
//
#include <pthread.h>

#include "ThreadPool.hh"
#include "threadTimers.hh"
#include <string>
#include <fstream>
#include <sstream>
#include <assert.h>
#include "test.h"
#include "knobs.hh"

#if LOG == 1
static std::ostringstream log_stream;
ThreadTimer *split_timer;
#define ACTIVE 0
#define NO_JOB 1
#define GET 2
#define ADD 3
#define DONE 4

ThreadCounter *tries;
#define NUM_TRIES 0;
#define SUM_NUM_TRIES 1;
#define NUM_SAMPLES 2;

long long int start_time,end_time;
#endif

__inline__
ull_t get_time ()  {
  #if TIMER_PRECISION==PRECISION_NANOSEC
  return get_time_nanosec();
  #endif
  #if TIMER_PRECISION==PRECISION_MICROSEC
  return get_time_microsec();
  #endif
  #if TIMER_PRECISION==PRECISION_TICKS
  return get_time_clockticks();
  #endif
}
 

#define DECLARE_TIMER_VARS		\
  volatile ull_t before=get_time();	\
  volatile ull_t after=0;

#define ADD_TIME_TO(x)					\
  after = get_time();					\
  split_timer->add(_thread_no, x, after-before);		\
  before = after;

#define RESET_TIMER				\
  after = get_time();				\
  before = after;

//Global threadpool
ThreadPool * thread_pool = NULL;

void sleep_for_nanoseconds (long int nanosecs) {
  timespec t,tr;
  t.tv_sec=nanosecs/(1000000000L);
  t.tv_nsec=nanosecs%(1000000000L);
  //nanosleep (&t,&tr);
  clock_nanosleep(CLOCK_THREAD_CPUTIME_ID,0,&t,&tr);
}

void
PoolThr::inf_loop () {
    _pool->_pthread_map[_thread_no] = syscall (__NR_gettid);

    _del_mutex.lock();
    #if LOG==1
    DECLARE_TIMER_VARS;
    #endif
    
    while ( !_pool->null_joined() ) {

      #if LOG == 1
      //tries->increment(_thread_no, SUM_NUM_TRIES);
      //tries->increment(_thread_no, NUM_TRIES);
      ADD_TIME_TO (NO_JOB);
      #endif

      //if (tries->get(_thread_no,NUM_TRIES) > 1)   // Not the first attempt
      //for (volatile int i=1; i<(1<<11); ++i); 
      
      if ( (_job=_pool->_scheduler->get(_thread_no)) != NULL) {
        #if LOG == 1
	//tries->increment(_thread_no,NUM_SAMPLES);
	//tries->reset(_thread_no,NUM_TRIES);
	ADD_TIME_TO (GET);
        #endif
      
	run_job();
      
        #if LOG == 1
	ADD_TIME_TO (ACTIVE);
        #endif
      }
  }
    
  _pool->_idle_cond.lock(); 
  _pool->_idle_count++;
  if (_pool->_idle_count == _pool->_max_parallel)
    _pool->_idle_cond.broadcast();
  _pool->_idle_cond.unlock();
  _done = true;
  _del_mutex.unlock();
}

// ThreadPool - implementation
ThreadPool::ThreadPool ( const uint max_p , Scheduler* scheduler, uint * proc_ids) {
  _max_parallel = max_p;
  _threads = new PoolThr*[ _max_parallel ];
  _idle_count = 0;
  _null_join = false;

  if (scheduler == NULL) // Use default scheduler
    _scheduler = new Scheduler(_max_parallel);
  else
    _scheduler = scheduler;
  if (_scheduler == NULL) {
    std::cerr<<"Scheduler can not be NULL"<<std::endl;
    exit(-1);
  }
  
  if ( _threads == NULL ) {
    _max_parallel = 0;
    std::cerr << "(ThreadPool) ThreadPool : could not allocate thread array" << std::endl;
  }

  _pthread_map = new pid_t [_max_parallel+1]; // The last one is for the main thread
  _pthread_map[_max_parallel] = syscall (__NR_gettid);
  _scheduler->set_pthread_map (_pthread_map);
  
  for ( uint i = 0; i < _max_parallel; i++ ) {
    _threads[i] = new PoolThr( i, this );
    
    
    if ( _threads == NULL )
      std::cerr << "(ThreadPool) ThreadPool : could not allocate thread" << std::endl;
    else
      if (_threads[i]->create( proc_ids==NULL ? -1 : proc_ids[i], true, true ) != 0) {
	exit(-1);
      }
  }
  //_inf_loop_cond.broadcast();
}

ThreadPool::~ThreadPool () {
  for ( uint i = 0; i < _max_parallel; i++ ) 
    _threads[i]->quit();                          // finish all thread

  for ( uint i = 0; i < _max_parallel; i++ ) {
    while (!_threads[i]->_done);
    _threads[i]->_del_mutex.lock();               // cancel and delete all threads (not really safe !)
    delete _threads[i];
  }
  
  delete[] _threads;
  delete _scheduler;
}

void
PoolThr::run_job() {
  if (_job == NULL)
    std::cerr<<"Error: thread tried to run a NULL job"<<std::endl;

  _job->set_thread (this);
  assert (_job->_executed == false);
  _job->run();                                 // execute job
  _job->_executed = true;
  
  _job->unlock();
  if (_job->deletable()) {
    delete _job;
    _job = NULL;
  }
}

ThreadPool*
PoolThr::get_pool () {
  return _pool;
}
 
void
PoolThr::quit () { // Quit thread (raise the _end flag to end inf_loop)
  _end      = true;
}

int
ThreadPool::set_thread_affinity (uint thread_id, uint proc_id) {
  if (thread_id > _max_parallel) {
    std::cerr<<"Threadpool has only "<<_max_parallel<<" threads, can not access thread: "
	     <<thread_id<<std::endl;
  }
  return _threads[thread_id]->set_affinity (proc_id);
}

void                                                
ThreadPool::add_jobs (int num_jobs, Job ** jobs, PoolThr* thr ) { 
  if ( jobs == NULL )
    return;
  
  for (int i=0;i<num_jobs;++i) 
    jobs[i]->lock();  // lock job for synchronisation
    
#if LOG==1
  long int before_add = get_time();
  #endif

  if (thr != NULL)
    _scheduler->add_multiple (num_jobs, jobs, thr->thread_no());
  else
    _scheduler->add_multiple (num_jobs, jobs, _max_parallel);
  
  #if LOG==1
  split_timer->add(thr!=NULL?thr->thread_no():0, ADD, get_time() - before_add);
  #endif
  
}

void                                                
ThreadPool::add_job ( Job * job, PoolThr* thr ) {
  this->add_jobs(1,&job,thr);
}

void                                                
ThreadPool::add_job ( Job * job,       
		      uint thread_id ) {
  if (thread_id >= _max_parallel) {
    std::cerr<<"Can not add job to thread "<<thread_id
	     <<", only "<<_max_parallel<<" threads in the system"<<std::endl;
    exit(-1);
  }
  add_job (job, _threads[thread_id]);
}

void
ThreadPool::done_job ( Job * job , PoolThr* thr , bool deactivate) {
  #if LOG==1
  long int before_done = get_time();
  #endif

  _scheduler->done (job, thr->thread_no(), deactivate);

  #if LOG==1
  split_timer->add(thr->thread_no(), DONE, get_time() - before_done);
  #endif
}

void
ThreadPool::change_idle_count (int diff) {
  _idle_cond.lock();                        // Update count of idle threads
  _idle_count+=diff;
  _idle_cond.unlock();
}  

// wait until <job> was executed
void
ThreadPool::sync ( Job * job ) {
  if ( job == NULL ) {
    std::cerr<<"Error: Tried syncing with null job, possibly deleted"<<std::endl;
    return;
  }
  if (job->deletable()) {
    std::cerr<<"Error: Tried syncing with a deletable job"<<std::endl;
    return;
  }
  
  job->lock();
  job->unlock();
}

// wait until all jobs have been executed
void
ThreadPool::sync_all () {
  _idle_cond.lock();
  _idle_cond.wait();
  std::cerr<<__func__<<std::endl;
  _idle_cond.unlock();
}

void
start_timers (uint num_procs) {
#if LOG == 1
  get_time();
  split_timer = new ThreadTimer(num_procs);
  tries = new ThreadCounter(num_procs);

#endif
}

void
print_timers (uint num_procs) {
#if LOG == 1
    for (int i=0; i<num_procs; ++i) {
      split_timer->subtract(i, ACTIVE, split_timer->get(i,ADD)+split_timer->get(i,DONE));
      if (0) {
    std::cout<<i
	     <<" active for "<<split_timer->get(i,ACTIVE)/1000000<<" ms,"
	     <<"\t get: "<<split_timer->get(i,GET)/1000000<<"ms"
	     <<"\t add: "<< split_timer->get(i,ADD)/1000000<<"ms"
	     <<"\t done: "<< split_timer->get(i,DONE)/1000000<<"ms"
	     <<"\t no_job: "<<split_timer->get(i,NO_JOB)/1000000<<"ms"
	     //<<"\t get_tries (fail): "<<((double)(*sum_num_tries)(i))/((double)(*num_samples)(i))
	     //<<"\t("<< (*sum_num_tries)(i)-(*num_samples)(i)  <<")"
	     <<std::endl;
      }}
  
  std::cout<<"get: "<<split_timer->avg(GET)/1000000<<" ms"<<std::endl;
  std::cout<<"add: "<<split_timer->avg(ADD)/1000000<<" ms"<<std::endl;
  std::cout<<"done: "<<split_timer->avg(DONE)/1000000<<" ms"<<std::endl;
  std::cout<<"emptyQ: "<<split_timer->avg(NO_JOB)/1000000<<"ms"<<std::endl;
  std::cout<<"Active(OH): "<<split_timer->avg(ACTIVE)/1000000<<"("
	   <<(split_timer->avg(NO_JOB)+split_timer->avg(GET)+split_timer->avg(ADD)+split_timer->avg(DONE))/1000000
	   <<")ms"<<std::endl;
  std::cout<<"Total: "<<(end_time-start_time)/1000000<<" ms  "<<std::endl;
  std::cout<<"ms_Active: "<<split_timer->avg(ACTIVE)/1000000<<std::endl;
  std::cout<<"ms_Overhead: "<<(split_timer->avg(NO_JOB)+split_timer->avg(GET)+split_timer->avg(ADD)+split_timer->avg(DONE))/1000000<<std::endl;
  
  //print_global_counters();
  //print_local_counters(num_procs);
    
#endif
}

int *counters;
// init global thread_pool
void
tp_init ( const uint p , uint * proc_ids, Scheduler * sched, Job * root) {

  #if LOG == 1
  start_timers(p);
  #endif
  
  if ( thread_pool != NULL )
    delete thread_pool;
 
  if ((thread_pool = new ThreadPool( p , sched, proc_ids)) == NULL)
    std::cerr << "(init_thread_pool) could not allocate thread pool" << std::endl;


  #if LOG == 1

  if (COUNTERS_ENABLED) {
    initPCM();
    before_sstate = getSystemCounterState();
  }

  before_ts = my_timestamp();

  start_time = get_time();
    split_timer->activate();
#endif
  tp_run (root);
}

// run job
void
tp_run ( Job * job ) {
  if ( job == NULL )
    return;
  thread_pool->set_null_join ();
  thread_pool->add_job( job );
}

// synchronise with specific job
void
tp_sync ( Job * job ) {
  thread_pool->sync( job );
}

// synchronise with all jobs
void
tp_sync_all () {
  thread_pool->sync_all();
#if LOG == 1
  split_timer->deactivate();
  end_time = get_time();
  // thread_pool->_scheduler->print_scheduler_stats();

  print_timers(thread_pool->max_parallel());
  if (COUNTERS_ENABLED)
    after_sstate = getSystemCounterState();
  after_ts = my_timestamp();
  
  if (COUNTERS_ENABLED) {
    std::cout<<"---------------------------------"<<std::endl;
    printDiff();
    std::cout<<"---------------------------------"<<std::endl;
  }
#endif
}

// finish thread pool
void
tp_done () {
  thread_pool->sync_all();
    
#if LOG == 1
  print_timers (thread_pool->max_parallel());
  std::ofstream log_file;
  log_file.open("Log");
  log_file<<log_stream.str();
  log_file.close();
#endif
  //delete thread_pool;
}

