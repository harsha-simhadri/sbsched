#ifndef __THREAD_HH
#define __THREAD_HH
//
//  Project   : ThreadPool
//  File      : Thread.hh
//  Author    : Ronald Kriemann
//  Purpose   : baseclass for a thread-able class
//
// arch-tag: d09c570a-520a-48ce-b612-a813b50e87b4
//

#include <sys/types.h>
#include <linux/unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include <iostream>

#define MY_SIGNAL 1
#define PTHREAD_SIGNAL 2
#define SIGNALING  PTHREAD_SIGNAL

// baseclass for all threaded classes: defines basic interface
class Thread {
protected:
  // thread-specific things
  pthread_t       _thread_id;
  pthread_attr_t  _thread_attr;
  
  bool            _running;                 // is the thread running or not
  const int       _thread_no;               // no of thread
  int             _proc_id;                 // Threads affinity set to this proc
  
public:
  Thread ( const int thread_no = -1 );
  virtual ~Thread ();

  int  thread_no     () const { return _thread_no; }
  void set_thread_no ( const int n );
  void reset_running () {_running = false;} // resets running-status (used in _run_proc, see Thread.cc)
   
  virtual void inf_loop () = 0;             // actual method the thread executes
  
  int  create (const uint proc_id = -1,     // proc (affinity) to stick the thread to
	       const bool detached = false,
	       const bool sscope = false ); // create thread (actually start it)
  void detach ();                           // detach thread
  void join   ();                           // synchronise with thread (wait until finished)

  int  set_affinity_attr (uint proc_id);    // Change the affinity attributes of the thead to proc_id
  int  set_affinity (uint proc_id);         // Change the affinity of the thead to proc_id
  uint proc_no () const { return _proc_id; }
  bool on_proc () const {                   // Is the thread on the proc it is supposed to run on?
    return ( _proc_id == -1
	     || _proc_id == sched_getcpu());
  }
  
  void cancel ();                           // request cancellation of thread
  
protected:
  void thread_exit   ();                    // terminate thread
  void sleep  ( const double sec );         // put thread to sleep for <sec> seconds
};

#if SIGNALING == MY_SIGNAL
#define LOCKED_STATE 1
#define UNLOCKED_STATE 0
// wrapper for pthread_mutex
class Mutex
{
protected:
  mutable pthread_spinlock_t m_spin;

public:
  Mutex () {  
        int error = pthread_spin_init(&m_spin, PTHREAD_PROCESS_PRIVATE);
        assert(!error);
    }
  
  ~Mutex () {  ; }

  // lock and unlock mutex (return 0 on success)
  int lock    () {
    int error = pthread_spin_lock( &m_spin  );
    assert(!error);
    return 0;
  }
  
  int unlock  () {
    int error = pthread_spin_unlock( &m_spin );
    assert(!error);
    return 0;
  }
 
  // return true if mutex is locked, otherwise false
  bool is_locked () {
    assert(false);
  }
};


// class for a condition variable
// - derived from mutex to allow locking of condition
//   to inspect or modify the predicate
class Condition : public Mutex {
protected:
    // our condition variable
  volatile uint ** locks;
  volatile uint start, end;  // Start and end of the threads spinning on locks

public:
  Condition  () {
    #define MAX_SLOTS 100
    #define SPREAD 32
    #define locks(i)  (locks[(i+1)*SPREAD])
    
    #define IDLE_STATE 0
    #define SPIN_STATE 1
    #define PROCEED_STATE 2
    locks = new volatile uint* [(2+MAX_SLOTS)*SPREAD];  // Have to handle dynamic allocation
    for (int i=0; i<MAX_SLOTS; ++i) {
      locks(i) = new volatile uint;
      *locks(i) = IDLE_STATE;
    }
    start = end = 0;
  }
    
  ~Condition () {
    for (int i=0; i<MAX_SLOTS; ++i) 
      delete locks(i);
    delete locks;
  }    
  
  void wait      () {
    volatile uint pos = end;
    end = (end+1) % MAX_SLOTS;
    
    if (end == start) {
      std::cerr<<"lock array in the condition object should be resized"<<std::endl;
      exit(-1);
    }
    *locks(pos) = SPIN_STATE;
    unlock();
    
    while (*locks(pos)==SPIN_STATE || __sync_bool_compare_and_swap (locks(pos), PROCEED_STATE, IDLE_STATE) == false);
    
    lock();
    return;
  }
    
  void signal    () {
    if (start == end) {
      return;
    } else {
      if (*locks(start) != SPIN_STATE) {
	std::cerr<<"Condition Signal expects lock to be in spin state"<<std::endl;
	exit(-1);
      }
      *locks(start) = PROCEED_STATE;
      start = (start+1)%MAX_SLOTS;
    }
  }
  
  void broadcast () {
    if (start == end) return;
    
    volatile uint real_end = (end<start) ? end+MAX_SLOTS : end;
    for (uint pos = start; pos<real_end; ++pos) {
      if (*locks(pos%MAX_SLOTS) != SPIN_STATE) {
	std::cerr<<"Condition Broadcast found lock to be in state: "<<*locks(pos)<<" at pos: "<<pos <<std::endl;
	exit(-1);
      }
      *locks(pos%MAX_SLOTS) = PROCEED_STATE;
    }
    start=end;
  }
  
};
#endif

#if SIGNALING == PTHREAD_SIGNAL
class Mutex
{
protected:
  pthread_mutex_t      _mutex;
  pthread_mutexattr_t  _mutex_attr;
  
public:
  Mutex () {
    pthread_mutexattr_init( & _mutex_attr );
    pthread_mutex_init( & _mutex, & _mutex_attr );
  }
  
  ~Mutex () {
    pthread_mutex_destroy( & _mutex );
    pthread_mutexattr_destroy( & _mutex_attr );
  }

  // lock and unlock mutex (return 0 on success)
  int lock    () { return pthread_mutex_lock(   & _mutex ); }
  int unlock  () { return pthread_mutex_unlock( & _mutex ); }
  
  // return true if mutex is locked, otherwise false
  bool is_locked () {
    if ( pthread_mutex_trylock( & _mutex ) != 0 )
      return true;
    else {
      unlock();
      return false;
    }
  }
};

// class for a condition variable
// - derived from mutex to allow locking of condition
//   to inspect or modify the predicate
class Condition : public Mutex {
protected:
    // our condition variable
    pthread_cond_t  _cond;

public:
  Condition  () { pthread_cond_init(    & _cond, NULL ); }
  ~Condition () { pthread_cond_destroy( & _cond ); }

  
  // condition variable related methods
  void wait      () { pthread_cond_wait( & _cond, & _mutex ); }
  void signal    () { pthread_cond_signal( & _cond ); }
  void broadcast () { pthread_cond_broadcast( & _cond ); }
};
#endif

#endif  // __THREAD_HH
