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

#define LOG 1

#if LOG == 1
static std::ostringstream log_stream;
ThreadTimer *active;
ThreadTimer *num_waits;
ThreadTimer *wait_time;
ThreadTimer *empty_jobq_time;
ThreadTimer *scheduler_overhead;
long int start_time;
#endif

#define DECLARE_TIMER_VARS			\
  long int before=0, after=0;

#define ADD_TIME_TO(x)				\
  after = get_time_microsec();			\
  x->add(_thread_no, after-before);		\
  before = after;

#define RESET_TIMER				\
  after = get_time_microsec();			\
  before = after;

//
// set to one to enable sequential execution
//
#define THR_SEQUENTIAL  0

//Global threadpool
ThreadPool * thread_pool = NULL;

void
PoolThr::inf_loop () {
  _del_mutex.lock();
  
  DECLARE_TIMER_VARS;

  while ( ! _end ) {
    // Loop can be rewritten to use only one "more" on average
#if LOG == 1
    ADD_TIME_TO(wait_time)
#endif
      _pool->_scheduler_cond.lock();
#if LOG == 1
    ADD_TIME_TO(wait_time);  //25% wait time
#endif
      
    if (_pool->_scheduler->more(_thread_no) == false) {
#if LOG == 1
      ADD_TIME_TO(scheduler_overhead);
      #endif
      _pool->_scheduler_cond.wait();
#if LOG == 1
      ADD_TIME_TO(empty_jobq_time);
      num_waits->add(_thread_no,1);
#endif
    }
    if (_pool->_scheduler->more(_thread_no) == true) {
#if LOG == 1
      ADD_TIME_TO(scheduler_overhead);
#endif 
      _job = _pool->_scheduler->get (_thread_no);
#if LOG == 1
      ADD_TIME_TO(scheduler_overhead);
#endif
      _pool->_scheduler_cond.unlock();

      _pool->change_idle_count(-1);

#if LOG == 1
      ADD_TIME_TO(wait_time);
#endif

      run_job ();

#if LOG == 1
      ADD_TIME_TO(active);
#endif

      _pool->change_idle_count(1);

#if LOG == 1
      ADD_TIME_TO(scheduler_overhead);
#endif
      _pool->_scheduler_cond.lock();
#if LOG == 1
      ADD_TIME_TO(wait_time);  // 65% wait time
      //RESET_TIMER;
#endif
      _pool->_idle_cond.lock();
#if LOG == 1
      ADD_TIME_TO(wait_time);  //negligible
#endif

      if (_pool->null_joined()) {
        #if LOG == 1
	//std::cout<<"Last job completed at: "<<after/1000<<" ms"<<std::endl;
        #endif
	if (_pool->_idle_count != _pool->_max_parallel) { // All threads are idle
	  std::cerr<<"Null joined while "<<_pool->_max_parallel-_pool->_idle_count
		   <<" non idle threads exist"<<std::endl;
	  //exit (-1);
	} if (_pool->_scheduler->more(-1)) { // No more jobs to do
	  std::cerr<<"Null joined while more jobs exist"<<std::endl;
	  exit (-1);
	}
	
	_pool->_idle_cond.broadcast();                 // Wake up sync_all function
	
      }
#if LOG == 1
      RESET_TIMER;
#endif
      _pool->_idle_cond.unlock();
      _pool->_scheduler_cond.unlock();
#if LOG == 1
      ADD_TIME_TO(wait_time);
#endif
    } else {
#if LOG == 1
      ADD_TIME_TO (scheduler_overhead);
#endif      
      _pool->_scheduler_cond.unlock();
#if LOG == 1
      ADD_TIME_TO(wait_time);
#endif      
    }
  }
  _done = true;
  _del_mutex.unlock();
}
// ThreadPool - implementation
ThreadPool::ThreadPool ( const uint max_p , Scheduler* scheduler, uint * proc_ids) {
  _max_parallel = max_p;
  _threads = new PoolThr*[ _max_parallel ];
  _idle_count = _max_parallel;
  _null_join = false;

  if (scheduler == NULL) // Use default scheduler
    _scheduler = new Scheduler(_max_parallel);
  else
    _scheduler = scheduler;
  
  if ( _threads == NULL ) {
    _max_parallel = 0;
    std::cerr << "(ThreadPool) ThreadPool : could not allocate thread array" << std::endl;
  }
    
  for ( uint i = 0; i < _max_parallel; i++ ) {
    _threads[i] = new PoolThr( i, this );
    
    if ( _threads == NULL )
      std::cerr << "(ThreadPool) ThreadPool : could not allocate thread" << std::endl;
    else
      if (_threads[i]->create( proc_ids==NULL ? -1 : proc_ids[i], true, true ) != 0) {
	exit(-1);
      }
  }

  // ??WHAT and the WHAT??
  // tell the scheduling system, how many threads to expect
  // (commented out since not needed on most systems)
  //     if ( pthread_setconcurrency( _max_parallel + pthread_getconcurrency() ) != 0 )
  //         std::cerr << "(ThreadPool) ThreadPool : pthread_setconcurrency ("
  //                   << strerror( status ) << ")" << std::endl;
}

ThreadPool::~ThreadPool () {
  for ( uint i = 0; i < _max_parallel; i++ ) 
    _threads[i]->quit();                          // finish all thread

  for ( uint i = 0; i < _max_parallel; i++ ) {
    while (!_threads[i]->_done)
      _scheduler_cond.broadcast();
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

  if (_job->_executed != 0) {
    std::cerr<<__func__<<" "<<_job->_executed<<std::endl;
    exit (-1);
  }
    
  _job->set_thread (this);
  _job->run();                                 // execute job
  
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
ThreadPool::add_job ( Job * job,       // Add job to scheduler's queue
		      PoolThr* thr ) { // If job is a cont_job, thr is the thread on which last job before 'join' was run
  if ( job == NULL )
    return;
  
  job->lock();  // lock job for synchronisation
  _scheduler_cond.lock();

  if (thr != NULL)
    _scheduler->add (job, thr->thread_no());
  else
    _scheduler->add (job, 0);
      	
  if (_idle_count > 0) 
    //_scheduler_cond.signal();
    _scheduler_cond.broadcast();
  
  _scheduler_cond.unlock();
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
  _scheduler_cond.lock();
  _scheduler->done (job, thr->thread_no(), deactivate);
  _scheduler_cond.unlock();
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
  //std::cerr<<__func__<<std::endl;
  _idle_cond.unlock();
}

void
start_timers (uint num_procs) {
#if LOG == 1
  get_time_microsec();
  active = new ThreadTimer(num_procs);
  num_waits = new ThreadTimer(num_procs);
  wait_time = new ThreadTimer(num_procs);
  scheduler_overhead = new ThreadTimer(num_procs);
  empty_jobq_time = new ThreadTimer(num_procs);
  start_time = get_time_microsec();
#endif
}

void
print_timers (uint num_procs) {
#if LOG == 1
    for (int i=0; i<num_procs; ++i) {
      std::cout<<i<<" active for "<<(*active)(i)/1000<<" ms,\twoken up "
	       <<(*num_waits)(i)<<" times,\t waited on jobQ for: "
	       <<(*empty_jobq_time)(i)/1000<<"ms,\t wait time: "
	       <<(*wait_time)(i)/1000<<"ms, \t sched overhead: "
	       <<(*scheduler_overhead)(i)/1000<<"ms"
	       <<std::endl;
    }
    std::cout<<"Average active time: "<<active->avg()/1000<<" ms"<<std::endl;
    std::cout<<"Average wait on job queue: "<<empty_jobq_time->avg()/1000<<" ms"<<std::endl;
    std::cout<<"Average wait time: "<<wait_time->avg()/1000<<" ms"<<std::endl;
    std::cout<<"Average sched overhead: "<<scheduler_overhead->avg()/1000<<" ms"<<std::endl;
    std::cout<<"Total run time: "<<(get_time_microsec()-start_time)/1000<<" ms"<<std::endl;
#endif
}

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

  tp_run (root);
}

// run job
void
tp_run ( Job * job ) {
  if ( job == NULL )
    return;

  //std::cout<<"No. of idle threads: "<<thread_pool->_idle_count<<std::endl;
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
  print_timers(thread_pool->max_parallel());
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
  delete thread_pool;
}

