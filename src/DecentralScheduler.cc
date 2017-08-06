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

#include "Scheduler.hh"
#include <assert.h>

bool
Scheduler::check_range (int thread_id, int start, int end,
						std::string * func_name) {
	if (thread_id<start || thread_id>=end ) {
		std::cerr << "Error: in "<< func_name << ", passed thread_id: "
		<<thread_id<<". Acceptable thread_id range: ["
		<<start<<", "<<end<<"), exiting"<<std::endl; 
		
		exit (-1);
	}
	delete func_name;
	return true;  
}

void
Scheduler::add (Job *job, int thread_id) {
	_queue_lock.lock();
	_job_queue.push_back(job);
	_queue_lock.unlock();
}

void
Scheduler::add_multiple (int num_jobs, Job **jobs, int thread_id) {
	_queue_lock.lock();
	for (int i=0; i<num_jobs; ++i)
	  _job_queue.push_back(jobs[i]);
	_queue_lock.unlock();
}

Job*
Scheduler::get (int thread_id) {
	
	Job* ret;
	_queue_lock.lock();
	if (_job_queue.size() == 0) {
		ret = NULL;
	} else {
		ret = _job_queue.front();
		_job_queue.erase (_job_queue.begin());
	}
	_queue_lock.unlock();  
	return ret;
}

bool
Scheduler::more (int thread_id) {
	std::cerr<<__func__<<" has been deprecated"<<std::endl;
	exit(-1);
}

void
Scheduler::print_scheduler_stats () {
}

void
Local_Scheduler::add(Job *job, int thread_id) {
	check_range (thread_id, 0, _num_threads+1, new std::string (__func__));
	++_num_jobs;
	if (thread_id == _num_threads)
	  _job_queues[0].push_back(job);    
	else
	  _job_queues[thread_id].push_back(job);
	
} 

void
Local_Scheduler::add_multiple(int num_jobs, Job **jobs, int thread_id) {
	check_range (thread_id, 0, _num_threads+1, new std::string (__func__));
	_num_jobs+=num_jobs;
	for (int i=0; i<num_jobs; ++i) {
	  if (thread_id == _num_threads)
	    _job_queues[0].push_back(jobs[i]);    
	  else
	    _job_queues[thread_id].push_back(jobs[i]);
	}
} 

Job*
Local_Scheduler::get (int thread_id) {
	// Make sure job Q is not empty
	assert (_num_jobs > 0);
	
	check_range (thread_id, 0, _num_threads, new std::string (__func__));
	
	--_num_jobs;
	
	if (_job_queues[thread_id].size() > 0) {
		Job * ret = _job_queues[thread_id].back();
		_job_queues[thread_id].pop_back();
		return ret;
	} else {
		std::cerr << "In "<< __func__ <<": job queue at processor "<<thread_id<<" empty, exiting"<<std::endl; 
		exit (-1);       
	}
}

bool
Local_Scheduler::more (int thread_id) {
	check_range (thread_id, -1, _num_threads, new std::string (__func__));
	
	if (thread_id == -1)
		return (_num_jobs>0);
	else 
		return ( _job_queues[thread_id].size() > 0 );
}

void
Local_Scheduler::done (Job *job, int thread_id,
					   bool deactivate) {
	// std::cout<<"Done job, thread_id: "<<thread_id<<std::endl;
}
