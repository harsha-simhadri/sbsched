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


#include "Fork.hh"

Fork::Fork (Fork *parent_fork, Job * parent_job,
	    int num_jobs, Job **children,
	    Job *cont_job) {
  _num_jobs = num_jobs;
  _num_synced_jobs = 0;
  _count_mutex = new Mutex;
  _parent_fork = parent_fork;
  _parent_job = parent_job;
  _parent_job_id = parent_job->get_id();
  
  _jobs = new Job*[_num_jobs];
  for (int i=0; i<_num_jobs; ++i) {
    _jobs[i] = children[i];
    _jobs[i]->_parent_fork = this;
  }

  _cont_job = cont_job;
  _cont_job->_parent_fork = _parent_fork;
  _cont_job->_strand_id = parent_job->_strand_id;
}

void
Fork::spawn (PoolThr * thr) {
  _thr = thr;
  _thr->get_pool()->done_job (_parent_job, _thr, false);

  for (int i=0; i<_num_jobs; ++i) 
    _jobs[i]->_strand_id = _jobs[i]->_id;
  
  //    _thr->get_pool()->add_job (_jobs[i], _thr); //Need to change this. This channels jobs through a single point. At this point this is also a livelock
  _thr->get_pool()->add_jobs (_num_jobs, _jobs, _thr); 
}

Fork::~Fork () {
//when is this called, and what can be deleted ??
}

int
Fork::join ( Job * job) {
  _count_mutex->lock ();
  _thr->get_pool()->done_job (job, job->get_thread(), true);
  if (++_num_synced_jobs == _num_jobs) {
    if (_cont_job == NULL) {
      std::cerr<<"No continuation job in sync"<<std::endl;
    } else { 
      _thr->get_pool()->add_job (_cont_job, job->_thread);
    }
    _count_mutex->unlock();
    return 0;
  } else {
    _count_mutex->unlock();
    return -1;
  }
}
