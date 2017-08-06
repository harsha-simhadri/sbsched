#include "Scheduler.hh"
#include "HR1Scheduler.hh"

#define VERBOSE 0


HR_Scheduler::Cluster::Cluster (const lluint size, const int block_size,
		  int num_children, 
		  Cluster * parent, Cluster ** children)
  : _size (size),
    _block_size (block_size) {
  _occupied=0;
  _active_set=NULL;
  _num_children=num_children;

  _parent=parent;
  _children = new Cluster* [_num_children];
  if (children!=NULL)
    for (int i=0; i<_num_children; ++i)
      _children[i]=children[i];
}

HR_Scheduler::Cluster*
HR_Scheduler::Cluster::create_tree( Cluster * root,
				    Cluster ** leaf_array,
				    int num_levels, uint * fan_outs,
				    lluint * sizes, uint * block_sizes) {
  static int leaf_counter=0;
  _locked_thread_id = -1;
  Cluster * ret;
  if (root == NULL) { // Create the RAM node
    ret = new Cluster (1<<((sizeof(lluint)*4)-1) -1, block_sizes[0],
		       fan_outs[0]);
    leaf_counter = 0;
    for (int i=0; i<fan_outs[0]; ++i) {
      ret->_children[i] = create_tree (ret, leaf_array,
				       num_levels-1, fan_outs+1,
				       sizes+1, block_sizes+1);
    }
  } else if (num_levels>0) {
    ret = new Cluster (sizes[0], block_sizes[0],
		       fan_outs[0], root);
    for (int i=0; i<fan_outs[0]; ++i) {
      ret->_children[i] = create_tree (ret, leaf_array,
				       num_levels-1, fan_outs+1,
				       sizes+1, block_sizes+1);
    }
  } else {
    ret = new Cluster (0, 1, 1, root); // L0 cache/register

    leaf_array[leaf_counter++] = ret;
  }
  return ret;
}


void
HR_Scheduler::print_tree( Cluster * root,
			  int num_levels, int total_levels) {
  if (total_levels == -1) total_levels=num_levels;
  if (num_levels > 0) {
    for (int i=0; i<total_levels-num_levels; ++i)
      std::cout<<"\t\t\t";

    if (root->_active_set!=NULL)
      std::cout<<"A,QL"<<root->_active_set->_task_queue.size()
	       <<",Pin"<<root->_active_set->_parent_job_id<<",";
    else
      std::cout<<"N,";
    if (root->_spawned_set!=NULL)
      std::cout<<"S"<<root->_spawned_set->_clusters_needed
	       <<",Par"<<root->_spawned_set->_parent_job_id<<",";
    else
      std::cout<<"S-";
    std::cout<<",cc"<<root->_occupied<<std::endl;
    
    for (int i=0; i<root->_num_children; ++i)
      print_tree (root->_children[i], num_levels-1, total_levels);
  }
}

void
HR_Scheduler::print_job (SizedJob* sized_job) {
  std::cout<<" Job: "<<sized_job->get_id()
	   <<", Str: "<<sized_job->strand_id()
	   <<", Pin: "<<sized_job->get_pin_id()
	   <<", Size: "<<sized_job->size(1)<<std::endl;
}

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
  _job_queue.push_back(job);
}

Job*
Scheduler::get (int thread_id) {
  Job* ret = _job_queue.front();
  _job_queue.erase (_job_queue.begin());
  return ret;
}

bool
Scheduler::more (int thread_id) {
  return ( _job_queue.size()>0 );
}

/*
void
WS_Scheduler::add (Job *job, int thread_id) {

  check_range (thread_id, 0, _num_threads, new std::string (__func__));
  ++_num_jobs;
  _job_queues[thread_id].push_back(job);

}

Job*
WS_Scheduler::get (int thread_id) {
  //static int steals=0;
  //std::cout<<"Getting job, num before getting: "<<_num_jobs<<", Thread Id: "<<thread_id<<std::endl;

  check_range (thread_id, 0, _num_threads, new std::string (__func__));

  --_num_jobs;
  if (_num_jobs<0) {
    std::cerr<<"Get called with no jobs in the system"<<std::cerr;
    exit (-1);
  }
  if (_job_queues[thread_id].size() > 0) {
    Job * ret = _job_queues[thread_id].back();
    _job_queues[thread_id].pop_back();
    return ret;
  } else {
    //std::cerr<<++steals<<std::endl;

    while (1) {
      int choice = (int) ((((double)rand())/((double)RAND_MAX))*_num_threads);
      if (_job_queues[choice].size() > 0) {
	Job * ret = _job_queues[choice].front();
	_job_queues[choice].erase(_job_queues[choice].begin());
      	return ret;
      }
    }
  }
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
*/

HR_Scheduler::HR_Scheduler (int num_threads, int num_levels,
			    int * fan_outs, lluint * sizes,
			    int * block_sizes, int type)
  : Scheduler (num_threads) {

  _tree->_num_levels=num_levels;
  _tree->_fan_outs=new uint[_tree->_num_levels];
  _tree->_sizes=new lluint[_tree->_num_levels];
  _tree->_block_sizes=new uint[_tree->_num_levels];

  _num_jobs=0;
 
  _tree->_num_leaves=1;
  for (int i=0; i<_tree->_num_levels; ++i) {
    _tree->_fan_outs[i] = fan_outs[i];
    _tree->_sizes[i] = sizes[i];
    _tree->_block_sizes[i] = block_sizes[i];

    _tree->_num_leaves*=fan_outs[i];
  }
  
  _tree->_leaf_array = new Cluster* [_tree->_num_leaves];
  _tree->_root = _tree->_root->create_tree (NULL, _tree->_leaf_array, _tree->_num_levels,
			      _tree->_fan_outs, _tree->_sizes, _tree->_block_sizes);

  _tree->_root->_active_set = NULL;
}

HR_Scheduler::~HR_Scheduler() {
  delete _tree->_fan_outs, _tree->_sizes, _tree->_block_sizes;
  // Delete the cluster tree too
}

void
HR_Scheduler::add (Job* job, int thread_id) {
   ++_num_jobs;
  
  SizedJob* sized_job = (SizedJob*)job;
  Cluster * cur;
  
  if (_tree->_root->_active_set == NULL) { // This is the first job added to the system, add it to the root
    _tree->_root->_active_set = new TaskSet (NULL, job->strand_id(), 
					_tree->_root->_num_children);
    cur = _tree->_root;
    sized_job->set_pin_id (sized_job->get_id());
  } else {
    for (cur = _tree->_leaf_array[thread_id];;cur = cur->_parent ) {
      if (cur->_active_set != NULL) {
	if (cur->_active_set->_parent_job_id == sized_job->get_pin_id()) 
	  break;
      }
    }
  }

  std::deque<SizedJob*> &task_queue = cur->_active_set->_task_queue;

  /*
  for (std::deque<SizedJob*>::iterator iter=task_queue.begin();
       iter != task_queue.end(); ++iter)
    if (sized_job->size(cur->_block_size) <= (*iter)->size(cur->_block_size)) {
      task_queue.insert (iter, sized_job);
#if VERBOSE >= 1
      std::cout<<__func__<<" "<<thread_id;
      print_job(sized_job);
      print_tree (_tree->_root, _tree->_num_levels+1);
#endif
      return;
    }*/
  task_queue.push_back (sized_job);

#if VERBOSE >= 1
  std::cout<<__func__<<" "<<thread_id;
  print_job(sized_job);
  print_tree (_tree->_root, _tree->_num_levels+1);
#endif
  return;
}

void
HR_Scheduler::done (Job *job, int thread_id, bool deactivate) {
  SizedJob* sized_job = (SizedJob*)job;
  Cluster * cur;
  
  for (cur = _tree->_leaf_array[thread_id];
       cur->_active_set == NULL;
       cur = cur->_parent) 
    cur->_occupied -= sized_job->size(cur->_block_size);
  cur->_occupied -= sized_job->size(cur->_block_size);
  
  //if ((sized_job->get_pin_id() == cur->_active_set->_parent_job_id)
  if ((sized_job->strand_id() == cur->_active_set->_parent_job_id)
      && deactivate) {
    ////Remove, not needed
    if (cur->_active_set->_task_queue.size() != 0) {
      print_tree (_tree->_root, _tree->_num_levels+1);
      std::cerr<<__func__<<": deleting a non-empty active set, jobs remaining: "
               <<cur->_active_set->_task_queue.size()<<std::endl;
      exit(-1);
    } 
    
    TaskSet * delete_set = cur->_active_set;
    if (cur->_parent!=NULL) {
      for (int i=0; i<cur->_parent->_num_children; ++i)
	if (cur->_parent->_children[i]->_active_set == delete_set)
	  cur->_parent->_children[i]->_active_set = NULL;
      if (cur->_parent->_spawned_set == delete_set)
	cur->_parent->_spawned_set = NULL;
      cur->_parent->_occupied -= sized_job->size(cur->_parent->_block_size);
    } else {
      cur->_active_set = NULL;
    }
    delete delete_set;
  }
  
  if (job->parent_fork()==NULL && deactivate) {
    std::cout<<"Unpinning the root"<<std::endl;
    delete _tree->_root->_active_set;
    _tree->_root->_active_set = NULL;
  }

#if VERBOSE >= 1
  if (deactivate) std::cout<<"DEACT, ";
  std::cout<<__func__<<" "<<thread_id;
  print_job (sized_job);
  print_tree (_tree->_root, _tree->_num_levels+1);
#endif
}

Job*
HR_Scheduler::get (int thread_id) {
  --_num_jobs;

  volatile int i;
  for (i=0; i<10000; ++i);
  
  Cluster * cur;
  for (cur = _tree->_leaf_array[thread_id];
       cur->_active_set == NULL;
       cur = cur->_parent);
  
  SizedJob * sized_job = cur->_active_set->_task_queue.front();
  sized_job->set_pin_id (cur->_active_set->_parent_job_id);

  if (cur->_active_set->_task_queue.size() == 0) {
    std::cerr<<"Pulling from an empty queue"<<std::endl;
    exit(-1);
  }  
  cur->_active_set->_task_queue.pop_front ();
  
  if (sized_job->size(1) <= 0) {
    std::cerr<<"Job size is: "<<sized_job->size(cur->_block_size)
	     <<". Size must be positive"<<std::endl;
    exit(-1);
  }

  for (cur = _tree->_leaf_array[thread_id];
       cur->_active_set == NULL;
       cur = cur->_parent)
    cur->_occupied += sized_job->size(cur->_block_size);
  cur->_occupied += sized_job->size(cur->_block_size);

#if VERBOSE >= 1
  std::cout<<__func__<<" Thr:"<<thread_id;
  print_job (sized_job);
  print_tree (_tree->_root, _tree->_num_levels+1);
#endif
  return sized_job;
}

bool
HR_Scheduler::more (int thread_id) {
  if (thread_id == -1)
    return _num_jobs;

#if VERBOSE >= 2
  std::cout<<__func__<<" "<<thread_id<<" #jobs:"<<_num_jobs<<std::endl;
  //print_tree (_tree->_root, _tree->_num_levels+1);
#endif
  
  Cluster* cur = _tree->_leaf_array[thread_id]->_parent;
  Cluster* prev = _tree->_leaf_array[thread_id]; 


  if (prev->_active_set != NULL)
    return (prev->_active_set->_task_queue.size() > 0);
/*
  if (cur->_active_set != NULL) {
    if (cur->_active_set->_task_queue.size() == 0) {
      return false;
    } else {
      if (cur->_occupied == 0)
	return true;
      return (cur->_active_set->_task_queue.at(0)->size (cur->_block_size)
	      < cur->_size - cur->_occupied);
    }
  }
*/
  
  do {
    if (prev->_active_set == NULL
	&& prev->_occupied > prev->_size) // Heavy strand from above being executed along the path
      return false;
    
    if (cur->_active_set != NULL) {
      std::deque<SizedJob*> *queue_ptr;
      if (cur->_spawned_set != NULL) {
        prev->_active_set = cur->_spawned_set;
        if (--prev->_active_set->_clusters_needed == 0)
          cur->_spawned_set = NULL;
        
        queue_ptr = &(prev->_active_set->_task_queue);
      } else {
        queue_ptr = &(cur->_active_set->_task_queue);
      }
      
      if (queue_ptr->size()==0)
        return false;
      
      SizedJob * sized_job = queue_ptr->at(0);
      
      if (queue_ptr == &(_tree->_leaf_array[thread_id]->_active_set->_task_queue))
        return queue_ptr->size();
      
      bool fit_strand = true;
      for (prev = _tree->_leaf_array[thread_id], cur = _tree->_leaf_array[thread_id]->_parent;
           prev->_active_set==NULL;
           prev = cur, cur = cur->_parent) {
        if (prev->_occupied > 0)
          fit_strand = false;
        
        lluint size = sized_job->size(cur->_block_size);
        if (size<=0) {
          std::cerr<<"Job size is: "<<sized_job->size(cur->_block_size)
                   <<". Size must be positive"<<std::endl;
          exit(-1);
        }
        
        if (size <= cur->_size - cur->_occupied) {
          if (size >  prev->_size) {
            prev->_active_set = new TaskSet (sized_job->parent_fork(),
                                               sized_job->strand_id(),
                                               allocate (size, prev->_size,
                                                         cur->_size, cur->_num_children));
            
            prev->_active_set->_task_queue.push_back (sized_job);
            
            cur->_occupied += size;
            if (--prev->_active_set->_clusters_needed > 0)
              cur->_spawned_set = prev->_active_set;
	    
            queue_ptr->erase (queue_ptr->begin());
            return fit_strand;
          } else {
            return false;  // Shouldn't this be true?
          }
        }     
      }
      return fit_strand;
    }
    
    prev = cur;
    cur = cur->_parent; 
  } while (cur != NULL);
  return false; // No job in the system
}

void
Local_Scheduler::add (Job *job, int thread_id) {
  check_range (thread_id, 0, _num_threads, new std::string (__func__));
  ++_num_jobs;
  _job_queues[thread_id].push_back(job);
}

Job*
Local_Scheduler::get (int thread_id) {
  if (_num_jobs == 0) {
    std::cerr << "In "<< __func__ <<": job queue empty, exiting"<<std::endl; 
    exit (-1);
  }
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
