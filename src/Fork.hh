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


#ifndef __FORK_HH
#define __FORK_HH

#include "Job.hh"
#include "ThreadPool.hh"

class Fork {
//protected:
public:
  Fork          *  _parent_fork;               // when is it NULL
  Job           *  _parent_job;                // This job ptr valid only until this fork is spawned
  lluint           _parent_job_id;             // This number may be reused later, depending on impelmentation

  PoolThr       *  _thr;                       // Thread on which fork has been called

  int              _num_synced_jobs;
  int              _num_jobs;
  Mutex         *  _count_mutex;              // Mutex to count number of returned threads
  
  Job           ** _jobs;                     // jobs to be spawned
  Job           *  _cont_job;                 // job to be run after all spawned jobs have returned
  
public:
  Fork ( Fork *parent_fork, Job * parent_job,
	 int num_jobs, Job **children,
	 Job *cont_job); 
  
  ~Fork ();

  void spawn (PoolThr * thr);                 // Spawn job in to the pool of this thread
  int  join  (Job * job);                     // Pass a pointer to the calling job
  Job* get_cont_job () {return _cont_job;}    // Return the continuation job 
};

#endif
