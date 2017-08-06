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


#include "WSScheduler.hh"
#include <assert.h>

void
WS_Scheduler::add (Job *job, int thread_id) {
	check_range (thread_id, 0, _num_threads+1, new std::string (__func__));
	if (thread_id != _num_threads) {
		_local_lock[thread_id].lock();
		_job_queues[thread_id].push_back(job);
		_local_lock[thread_id].unlock();
	} else {   // External actor is putting job in the system
		_local_lock[0].lock();
		_job_queues[0].push_back(job);
		_local_lock[0].unlock();
	}
}
void
WS_Scheduler::add_multiple (int num_jobs, Job **jobs, int thread_id) {
	check_range (thread_id, 0, _num_threads+1, new std::string (__func__));
	if (thread_id != _num_threads) {
	  for (int i=0; i<num_jobs; ++i) {
		_local_lock[thread_id].lock();
		_job_queues[thread_id].push_back(jobs[i]);
		_local_lock[thread_id].unlock();
	  }
	} else {   // External actor is putting job in the system
	  for (int i=0; i<num_jobs; ++i) {
		_local_lock[0].lock();
		_job_queues[0].push_back(jobs[i]);
		_local_lock[0].unlock();
	  }
	}
}

int 
WS_Scheduler::steal_choice (int thread_id) {
  return  (int) ((((double)rand())/((double)RAND_MAX))*_num_threads);
}

Job*
WS_Scheduler::get (int thread_id) {
	//static int steals=0;
	//std::cout<<"Getting job, num before getting: "<<_num_jobs
	//         <<", Thread Id: "<<thread_id<<std::endl;
	check_range (thread_id, 0, _num_threads, new std::string (__func__));
	
	_local_lock[thread_id].lock();
	if (_job_queues[thread_id].size() > 0) {
		Job * ret = _job_queues[thread_id].back();
		_job_queues[thread_id].pop_back();
		_local_lock[thread_id].unlock();
		return ret;
	} else {
		//std::cerr<<++steals<<std::endl;
		_local_lock[thread_id].unlock();
		for (int i=0; i<1; ++i) {
			
		        int choice = steal_choice(thread_id);
			_steal_lock[choice].lock();
			_local_lock[choice].lock();
			if (_job_queues[choice].size() > 0) {
				Job * ret = _job_queues[choice].front();
				_job_queues[choice].erase(_job_queues[choice].begin());
				++_num_steals[thread_id];
				_local_lock[choice].unlock();
				_steal_lock[choice].unlock();
				return ret;
			}
			_local_lock[choice].unlock();
			_steal_lock[choice].unlock();
			//for (volatile int i=0; i<100; ++i);  // Volatile to make sure loop runs under -O flag,
		}
	}
	return NULL;
}

bool
WS_Scheduler::more (int thread_id) {
	check_range (thread_id, -1, _num_threads, new std::string (__func__));
	
	return ( _num_jobs > 0 );
}

void
WS_Scheduler::done (Job *job, int thread_id,
					bool deactivate) {
	// std::cout<<"Done job, thread_id: "<<thread_id<<std::endl;
}

WS_Scheduler::~WS_Scheduler () {
  delete _job_queues;
}

void
WS_Scheduler::print_scheduler_stats () {
  lluint total_steals;
  for (int i=0; i<_num_threads; ++i)
    total_steals += _num_steals[i];

  std::cout<<"Total number of steals"<<std::endl;
}

int 
PWS_Scheduler::steal_choice (int thread_id) {

  double fraction = ((double)rand())/((double)RAND_MAX);
  int cluster_id = thread_id/_cluster_size;
  int choice;
  
  double threshold1 = _small * cluster_id * _cluster_size;
  double threshold2 = threshold1 + _cluster_size * _large;

  if (fraction < threshold1) {
    choice = ((int)(fraction/_small));
  } else if (fraction < threshold2) {
    choice = (int)((fraction-threshold1)/_large) + _cluster_size*cluster_id;
  } else {
    choice = (int)((fraction-threshold2)/_small) + _cluster_size*(cluster_id+1);
  }
  
  assert (choice<_num_threads);
  return choice;
}

Job*
PWS_Scheduler::get (int thread_id) {
	//static int steals=0;
	//std::cout<<"Getting job, num before getting: "<<_num_jobs
	//         <<", Thread Id: "<<thread_id<<std::endl;
	check_range (thread_id, 0, _num_threads, new std::string (__func__));
	
	_local_lock[thread_id].lock();
	if (_job_queues[thread_id].size() > 0) {
		Job * ret = _job_queues[thread_id].back();
		_job_queues[thread_id].pop_back();
		_local_lock[thread_id].unlock();
		return ret;
	} else {
		//std::cerr<<++steals<<std::endl;
		_local_lock[thread_id].unlock();
		for (int i=0; i<1; ++i) {
			
		        int choice = steal_choice(thread_id);
			_steal_lock[choice].lock();
			_local_lock[choice].lock();
			if (_job_queues[choice].size() > 0) {
				Job * ret = _job_queues[choice].front();
				_job_queues[choice].erase(_job_queues[choice].begin());
				++_num_steals[thread_id];
				_local_lock[choice].unlock();
				_steal_lock[choice].unlock();
				return ret;
			}
			_local_lock[choice].unlock();
			_steal_lock[choice].unlock();
			//for (volatile int i=0; i<100; ++i);  // Volatile to make sure loop runs under -O flag,
		}
	}
	return NULL;
}
