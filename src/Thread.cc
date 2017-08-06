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
//  Project   : ThreadPool
//  File      : TThread.cc
//  Author    : Ronald Kriemann
//  Purpose   : baseclass for a thread-able class
//
// arch-tag: aec71576-f8d5-4573-ad31-f3dbddc59934
//

#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#include <iostream>
#include <cmath>

#include "Thread.hh"

//
// routine to call Thread::run() method
//
extern "C"
void *
_run_thread ( void *arg ) {
  if (arg != NULL) {
    ((Thread*) arg)->inf_loop();
    ((Thread*) arg)->reset_running();
  }

  return NULL;
}
    
Thread::Thread ( const int thread_no )
  : _running( false ),
    _thread_no(thread_no) {}

Thread::~Thread () {
  if ( _running )
    cancel();
}

int
Thread::set_affinity_attr (uint proc_id) {
  int status;
  
  if ( proc_id != -1 ) {
    cpu_set_t affinity;
    CPU_ZERO ( &affinity );
    CPU_SET ( proc_id, &affinity );
    if ((status = pthread_attr_setaffinity_np ( & _thread_attr, sizeof(affinity), &affinity))
	!= 0) {
      std::cerr << "Setting thread affinity to "<<proc_id<<" failed with error value "
		<< strerror ( status ) <<std::endl;
    } 
  }
  
  if (status == 0) 
    _proc_id = proc_id;

  return status;
}

int
Thread::set_affinity (uint proc_id) {
  int status;
  
  if ( proc_id != -1 ) {
    cpu_set_t affinity;
    CPU_ZERO ( &affinity );
    CPU_SET ( proc_id, &affinity );
    if ((status = pthread_setaffinity_np ( _thread_id, sizeof(affinity), &affinity))
	!= 0) {
      std::cerr << "Setting thread affinity to "<<proc_id<<" failed with error value "
		<< strerror ( status ) <<std::endl;
    } 
  }
  
  if (status == 0) 
    _proc_id = proc_id;

  return status;
}

// create thread (actually start it)
int 
Thread::create ( const uint proc_id, const bool detached, const bool sscope ) {
  if ( ! _running ) {
    int status;
    
    if ((status = pthread_attr_init( & _thread_attr )) != 0) {
      std::cerr << "(Thread) create : pthread_attr_init ("
		<< strerror( status ) << ")" << std::endl;
      return status;
    }

    if ( detached ) {
      // detache created thread from calling thread
      if ((status = pthread_attr_setdetachstate( & _thread_attr,
						 PTHREAD_CREATE_DETACHED )) != 0) {
	std::cerr << "(Thread) create : pthread_attr_setdetachstate ("
		  << strerror( status ) << ")" << std::endl;
	return status;
      }
    }

    if (sscope) {
      // use system-wide scheduling for thread
      if ((status = pthread_attr_setscope( & _thread_attr, PTHREAD_SCOPE_SYSTEM )) != 0 ) {
	std::cerr << "(Thread) create : pthread_attr_setscope ("
		  << strerror( status ) << ")" << std::endl;
	return status;
      }
    }

    if ((status = set_affinity_attr (proc_id)) !=0)
      return status;
    _proc_id = proc_id;
    
#if defined(_POSIX_THREAD_PRIORITY_SCHEDULING) && defined(SUNOS)
        //
        // adjust thread-scheduling for Solaris
        //
    
    struct sched_param  t_param;
    
    t_param.sched_priority = sched_get_priority_min( SCHED_RR );
    
    if ((status = pthread_attr_setschedpolicy(  & _thread_attr, SCHED_RR )) != 0)
      std::cerr << "(Thread) create : pthread_attr_setschedpolicy ("
		<< strerror( status ) << ")" << std::endl;
    
    if ((status = pthread_attr_setschedparam(   & _thread_attr, & t_param )) != 0)
      std::cerr << "(Thread) create : pthread_attr_setschedparam ("
		<< strerror( status ) << ")" << std::endl;
    
    if ((status = pthread_attr_setinheritsched( & _thread_attr, PTHREAD_EXPLICIT_SCHED )) != 0)
      std::cerr << "(Thread) create : pthread_attr_setinheritsched ("
		<< strerror( status ) << ")" << std::endl;
#endif

#ifdef HPUX
    // on HP-UX we increase the stack-size for a stable behaviour
    // (need much memory for this !!!)
    pthread_attr_setstacksize( & _thread_attr, 32 * 1024 * 1024 );
#endif
    
    if ((status = pthread_create( & _thread_id, & _thread_attr, _run_thread, this )
	 != 0)) {
      std::cerr << "(Thread) create : pthread_create ("
		<< strerror( status ) << ")" << std::endl;
      return status;
    } else {
      _running = true;
      return 0;
    }
  } else {
    std::cout << "(Thread) create : thread is already running" << std::endl;
    return 0;
  }
}


// detach thread
void 
Thread::detach () {
  if ( _running ) {
    int status;
    if ((status = pthread_detach( _thread_id )) != 0)
      std::cerr << "(Thread) detach : pthread_detach ("
		<< strerror( status ) << ")" << std::endl;
  }
}

// synchronise with thread (wait until finished)
void 
Thread::join () {
  if ( _running ) {
    int status;
    
    // wait for thread to finish
    if ((status = pthread_join( _thread_id, NULL )) != 0)
      std::cerr << "(Thread) join : pthread_join ("
		<< strerror( status ) << ")" << std::endl;
    
    _running = false;
  }
}

// request cancellation of thread
void 
Thread::cancel () {
  if ( _running ) {
    int status;
    
    if ((status = pthread_cancel( _thread_id )) != 0)
      std::cerr << "(Thread) cancel : pthread_cancel ("
		<< strerror( status ) << ")" << std::endl;
  }
}

////////////////////////////////////////////
//
// functions to be called by a thread itself
//

//
// terminate thread
//
void 
Thread::thread_exit () {
  if ( _running && (pthread_self() == _thread_id)) {
    void  * ret_val = NULL;
    pthread_exit( ret_val );
    _running = false;
  }
}

//
// put thread to sleep (milli + nano seconds)
//
void
Thread::sleep ( const double sec ) {
  if ( _running ) {
    struct timespec  interval;
    
    if ( sec <= 0.0 ) {
      interval.tv_sec  = 0;
      interval.tv_nsec = 0;
    } else {
      interval.tv_sec  = time_t( std::floor( sec ) );
      interval.tv_nsec = long( (sec - interval.tv_sec) * 1e6 );
    }
    
    nanosleep( & interval, 0 );
  }
}
