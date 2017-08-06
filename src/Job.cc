#include "Job.hh"
#include "Fork.hh"
#include "ThreadPool.hh"

void
Job::run () {
  function ();
  if (!_fork_or_sync)
    std::cerr<<"Job finished with out forking or syncing"<<std::endl;
}

void
Job::fork (int num_jobs, Job **children, 
	   Job *cont_job) {
  _fork_or_sync = true;
  Fork* new_fork = new Fork (_parent_fork, this,
			     num_jobs, children,
			     cont_job );
  new_fork->spawn(_thread);
}

void
Job::binary_fork (Job* child0, Job* child1,
		  Job* cont_job) {
  Job** new_jobs = new Job*[2];
  new_jobs[0] = child0;
  new_jobs[1] = child1;
  fork (2, new_jobs, cont_job);
}

void
Job::unary_fork (Job* child,
		 Job* cont_job) {
  Job** new_jobs = new Job*[1];
  new_jobs[0] = child;
  fork (1, new_jobs, cont_job);
}

void
Job::join () {
  _fork_or_sync = true;
  if (_parent_fork != NULL) {
    _parent_fork->join ( this );
  } else {
    _thread->get_pool()->done_job (this, _thread, true);
    _thread->get_pool()->reset_null_join();
//    std::cout<<"Joining to a null fork"<<std::endl;
  }
}

Job*
SizedJob::cast (Job *job, bool exit_on_fail) {
  SizedJob * j = dynamic_cast<SizedJob*>(job);
  if (j==NULL) {
    if (exit_on_fail) {
      std::cerr<<"Error: HR_Scheduler needs a job of type SizedJob"<<std::endl;
      exit(-1);
    } else {
      return NULL;
    }
  } else {
    return j;
  }
}

void
SizedJob::fork(int num_jobs, Job **children, 
	       Job *cont_job) {
  ((SizedJob*)cast(cont_job))->_pin_id = _pin_id;
  
  for (int i=0; i<num_jobs; ++i)
    ((SizedJob*)cast(children[i]))->_pin_id = _pin_id;
    
  Job::fork (num_jobs, children, cont_job);
}
