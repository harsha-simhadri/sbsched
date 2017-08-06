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


#ifndef __JOB_HH
#define __JOB_HH

#include "Thread.hh"
#include "math.h"
#include <stdint.h>

class PoolThr;
class Fork;
class Job;

typedef unsigned int uint;
//typedef long long unsigned int lluint;
typedef long long int lluint;

static volatile int job_counter=0;
// class for a job in the pool
class Job {
  friend class Fork;
public:
  Mutex              _sync_mutex;              // mutex for synchronisation

  lluint             _id;                      // Unique id, up to scheduler to use this
  
  Fork	       *     _parent_fork;             // Fork that spawned this job, used to sync to or pass on to children
  lluint             _strand_id;               // Which job_id started this job
  
  PoolThr      *     _thread;
  bool               _fork_or_sync;            // Did this job fork or sync at the end?
  bool               _delete;                  // Delete after completion? This feature can be used to keep root with out deletion??

public:
  int               _executed;                // Has the job been executed

  Job ( bool del = true )
    : _parent_fork (NULL),
      _strand_id (-1),
      _thread (NULL),
      _fork_or_sync (false),
      _delete (del),
      _executed (0)
    {
        _id = __sync_add_and_fetch(&job_counter, 1);
    }
  virtual ~Job () {
    //Need to delete job
    if ( _sync_mutex.is_locked() )
      std::cerr << "(Job) destructor : job is still running!" << std::endl;
  }

  virtual void function() = 0;
  
  void     run         ();
  virtual
  void     fork        (int num_jobs, Job **children, Job *cont_job);
  void     binary_fork (Job* child0, Job* child1, Job *cont_job);
  void     unary_fork  (Job* child, Job *cont_job);
  void     join        ();

  virtual
  Job*     cast        () { return this; }                   // In derived classes, check if object is of derived class
  
  bool     deletable   () { return _delete;}
  void     set_thread  (PoolThr* thr) {_thread=thr;}
  PoolThr* get_thread  () {return _thread;}

  void     set_id      (lluint id) {_id=id;}
  lluint   get_id      () {return _id;}
  
  Fork   * parent_fork () {return _parent_fork;}
  lluint   strand_id   () {return _strand_id;}
  bool     is_cont_job () {return _id!=_strand_id;}      // If this Job a continuation strand in a job
  
  // Mutex
  void     lock        () {}// std::cout << "Job " << _id << " lock" << std::endl; _sync_mutex.lock(); }
  void     unlock      () {}// std::cout << "Job " << _id << " unlock" << std::endl;  _sync_mutex.unlock(); }
};

class SizedJob : public Job {
  lluint              _pin_id;
public:
  SizedJob (bool del = true)
    : Job (del),
      _pin_id(-1)
    {}
  
  void     fork        (int num_jobs, Job **children,
			Job *cont_job);
  
  Job*     cast        (Job* job, bool exit_on_fail=true); // Over ride exit_on_fail if you want program to continue running despite a negative result
  
  virtual  lluint size (const int block_size) = 0;
   inline   lluint round_up (lluint size, const int block_size) {
    return (lluint)ceil(((double)size/(double)block_size))*block_size;
  }

  lluint   get_pin_id  () {return _pin_id;}
  void     set_pin_id  (lluint pin_id) {_pin_id = pin_id;}

  class Size {
  public:
    inline lluint operator()(SizedJob *job, int block_size=1) {
      return job->size(block_size);
    }
  };
};


class HR2Job;
typedef class HR2Job : public SizedJob {

  #define        STRAND_SIZE 100    // Check and update these numbers with the real values
  
  void*       _pin_cluster; // This and the following should be typed HR2Scheduler::Cluster*
  void*       _parent_pin_cluster;
public:
  HR2Job (bool del = true)
    : SizedJob (del),
      _pin_cluster(NULL),
      _parent_pin_cluster(NULL)
    {}

  virtual lluint strand_size (const int block_size)=0;
  
  void fork (int num_jobs, Job **children, Job *cont_job) {
    ((HR2Job*)cast(cont_job))->_pin_cluster = _pin_cluster;
    ((HR2Job*)cast(cont_job))->_parent_pin_cluster = _parent_pin_cluster;
    
    for (int i=0; i<num_jobs; ++i) {
      ((HR2Job*)cast(children[i]))->_pin_cluster = _pin_cluster;
      ((HR2Job*)cast(children[i]))->_parent_pin_cluster = _pin_cluster;
    }

    Job::fork (num_jobs, children, cont_job);
  }
  
  Job* cast (Job* job, bool exit_on_fail=true) {
    HR2Job* j = dynamic_cast<HR2Job*>(job);
    if (j==NULL) {
      std::cerr<<"Error: HR_Scheduler needs a job of type HR2Job"<<std::endl;
      exit(-1);
    } else {
      return j;
    }
  }

  bool is_maximal() {
     return (_pin_cluster != _parent_pin_cluster);
  }

  bool is_parent_pin_null () {
    return _parent_pin_cluster==NULL;
  }

  void  pin_to_cluster  (void* cluster, lluint size) {_pin_cluster = cluster; }
  void* get_pin_cluster  () {return _pin_cluster;}

} HR2Job;


#endif 
