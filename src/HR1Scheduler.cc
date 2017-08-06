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


#include "HR1Scheduler.hh"
#include <assert.h>

//#define NDEBUG

HR_Scheduler::Cluster::Cluster (const lluint size, const int block_size,
		  int num_children, Cluster * parent,
		  Cluster ** children)
  : _size (size),
    _block_size (block_size) {
	_occupied=0;
	_active_set=NULL;
	_spawned_set=NULL;
	_num_children=num_children;
	_parent=parent;
	_locked_thread_id = -1;
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
  
  Cluster * ret;
  if (root == NULL) { // Create the RAM node
    ret = new Cluster (1<<((sizeof(lluint)*4)-1) -1,
				     block_sizes[0], fan_outs[0]);
    leaf_counter = 0;
    for (int i=0; i<fan_outs[0]; ++i)
      ret->_children[i] = create_tree (ret, leaf_array,
				       num_levels-1, fan_outs+1,
				       sizes+1, block_sizes+1);
  } else if (num_levels>0) {
    ret = new Cluster (sizes[0], block_sizes[0],
				     fan_outs[0], root);
    for (int i=0; i<fan_outs[0]; ++i)
      ret->_children[i] = create_tree (ret, leaf_array,
				       num_levels-1, fan_outs+1,
				       sizes+1, block_sizes+1);
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
		 <<", Size: "<<sized_job->size(1)
		 <<std::endl;
}

void
HR_Scheduler::print_info (int thread_id, SizedJob* sized_job, int deactivate=-1) {
  _print_lock.lock();

  std::cout<<"Thread_id: "<<thread_id;
  if (deactivate!=-1)
    std::cout<<" deact:"<<deactivate;
  std::cout<<std::endl;
  
  print_job (sized_job);
  print_tree (_tree->_root, 3,3);

  _print_lock.unlock();
}

HR_Scheduler::HR_Scheduler (int num_threads, int num_levels,
			    int * fan_outs, lluint * sizes,
			    int * block_sizes, int type)
: Scheduler (num_threads) {
        _type = 0;
	std::cout<<_type<<std::endl;
	
        _tree = new TreeOfCaches;
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
	
	_tree->_root->set_active_set(NULL, -1);
	
	_tree->_num_locks_held  = new int      [_tree->_num_leaves];
	_tree->_locked_clusters = new Cluster** [_tree->_num_leaves];
	for (int i=0; i<_tree->_num_leaves; ++i) {
		_tree->_num_locks_held[i] = 0;
		_tree->_locked_clusters[i] = new Cluster*[_tree->_num_levels+1];
		for (int j=0; j<_tree->_num_levels+1; ++j)
			_tree->_locked_clusters[i][j] = NULL;
	}
}

HR_Scheduler::~HR_Scheduler() {
	for (int i=0; i<_tree->_num_leaves; ++i) delete _tree->_locked_clusters[i];
	delete _tree->_fan_outs;
	delete _tree->_sizes;
	delete _tree->_block_sizes;
	delete _tree->_num_locks_held;
	delete _tree->_locked_clusters;
	// Delete the cluster tree too
}

// Basic sanity check on locks held by a thread 
void
HR_Scheduler::check_lock_consistency (int thread_id) {
  for (int i=0; i<_tree->_num_levels+1; ++i) {
    if (i<_tree->_num_locks_held[thread_id])
      assert (_tree->_locked_clusters[thread_id][i] != NULL);
    else
      assert (_tree->_locked_clusters[thread_id][i] == NULL);
    
    if (i != _tree->_num_levels+1)
      if (_tree->_locked_clusters[thread_id][i] != NULL)
	assert  (_tree->_locked_clusters[thread_id][i]
		 != _tree->_locked_clusters[thread_id][i+1]);
  }
}

// Lock up 'node', and add that lock to the locked up node list
void
HR_Scheduler::lock (Cluster* node, int thread_id) {
	
	node->lock();
	assert(node->_locked_thread_id == -1);

	node->_locked_thread_id = thread_id;
	_tree->_locked_clusters[thread_id][_tree->_num_locks_held[thread_id]] = node;
	++_tree->_num_locks_held[thread_id];
}

bool
HR_Scheduler::has_lock (Cluster* node, int thread_id) {
    return node->_locked_thread_id == thread_id;
}

void
HR_Scheduler::print_locks (int thread_id) {
	std::cout<<"Thread "<<thread_id<<" has "<<_tree->_num_locks_held[thread_id]
		 <<" locks: ";
	for (int i=0; i<_tree->_num_locks_held[thread_id]; ++i) {
	  std::cout<<" '"<<_tree->_locked_clusters[thread_id][i];
	}
	std::cout<<std::endl;
}


// Check if the node is the last in the list of locked nodes,
void
HR_Scheduler::unlock (Cluster* node, int thread_id) {

	--_tree->_num_locks_held[thread_id];
	_tree->_locked_clusters[thread_id][_tree->_num_locks_held[thread_id]] = NULL;
	assert(node->_locked_thread_id == thread_id);
	node->_locked_thread_id = -1;
	node->unlock();
}

// Release all locks held by thread in the inverse order they were obtained
void
HR_Scheduler::release_locks (int thread_id) {
	while (_tree->_num_locks_held[thread_id] > 0)
		unlock (_tree->_locked_clusters[thread_id][_tree->_num_locks_held[thread_id]-1], thread_id);
}

HR_Scheduler::Cluster*
HR_Scheduler::find_active_set (int thread_id, SizedJob *sized_job) {
	for (Cluster *cur = _tree->_leaf_array[thread_id];;cur = cur->_parent ) {
		lock (cur, thread_id);

		if (cur->_active_set != NULL) {

		  // AAPO: should we lock cur->_active_set before looking at _parent_job_id?
			if (cur->_active_set->_parent_job_id == sized_job->get_pin_id())  {
				cur->_active_set->lock();
				return cur;
			}
		}
	}
	return NULL;
}

inline void
HR_Scheduler::insert_task_in_queue (std::deque<SizedJob*> &task_queue, SizedJob *sized_job, lluint block_size) {
  /*for (std::vector<SizedJob*>::iterator iter=task_queue.begin();iter != task_queue.end(); ++iter)
		if (sized_job->size(block_size) <= (*iter)->size(block_size)) {
			task_queue.insert (iter, sized_job);
			return;
		}*/
	task_queue.push_back (sized_job);
}

void
HR_Scheduler::add (Job* job, int thread_id) {
  ++_num_jobs;  // REMOVE THIS
	
	SizedJob* sized_job = (SizedJob*)job;
	Cluster *root = _tree->_root;

	/* Job added by an agent other than the threads */
	if (thread_id == _num_threads) {
	        root->lock ();
		root->_active_set = new TaskSet (NULL,job->strand_id(),root->_num_children);
		sized_job->set_pin_id (sized_job->get_id());
		root->_active_set->_task_queue.push_back (sized_job);
		root->unlock ();
		return;
	}
	
	/* If root has no active job, set it here */
	if (root->_active_set == NULL) {
		for (Cluster *cur = _tree->_leaf_array[thread_id];cur->_parent!=NULL;cur = cur->_parent)
			lock (cur, thread_id); 	// Lock up the path to the root
		if (root->_active_set == NULL) {
			root->_active_set = new TaskSet (NULL,job->strand_id(),root->_num_children);
			sized_job->set_pin_id (sized_job->get_id());
		}
		release_locks (thread_id);
	}

	Cluster * cur = find_active_set (thread_id, sized_job);
	insert_task_in_queue (cur->_active_set->_task_queue, sized_job, cur->_block_size);
	cur->_active_set->unlock();
	release_locks(thread_id);

	print_info (thread_id, sized_job);
	return;
}

// Require that node's active_set be locked, and that the active_set be completed
void
HR_Scheduler::release_active_set (Cluster *node, int thread_id) {
	int del = node->_active_set->change_clusters_attached(-1);
	TaskSet * tmp_active_set = node->_active_set;
	node->set_active_set(NULL, thread_id);
	tmp_active_set->unlock();      
	if (del == 0) 
		delete tmp_active_set;  // Aapo: why not recycle? It is dangerous to set pointers null.
}

void
type_1_done_ext () {
/* Alt HR: Check cur->_parent->vec and delete entry */			  
/*  assert (cur->_parent->_spawned_set_vector.size() > 0);
			      std::vector<TaskSet*>::iterator iter = cur->_parent->_spawned_set_vector.begin();
			      for (;iter!=cur->_parent->_spawned_set_vector.end();++iter) {
				    if (*iter == cur->_active_set) {
				          cur->_parent->_spawned_set_vector.erase (iter);
					  break;
				    }
				    assert (iter != cur->_parent->_spawned_set_vector.end());
			      } 
*/
}

void
HR_Scheduler::done (Job *job, int thread_id, bool deactivate) {
	SizedJob* sized_job = (SizedJob*)job;
	Cluster * cur  = _tree->_leaf_array[thread_id];

	/* Update occupied size count */
	lock (cur, thread_id);
	for (;cur->_active_set == NULL;cur = cur->_parent) {
		lock (cur->_parent, thread_id);
		cur->_occupied -= sized_job->size(cur->_block_size);
	}
	cur->_occupied -= sized_job->size(cur->_block_size);

	/* If the done task corresponds to an active set, clear it */
	if ((sized_job->strand_id() == cur->_active_set->_parent_job_id)
		&& deactivate) {
	      cur->_active_set->lock();
 		    if (cur->_parent!=NULL) {
			    lock (cur->_parent, thread_id);
			    //if (_type == 1 && cur->_parent->_num_children>1) type_1_done_ext();
			    if (cur->_parent->_spawned_set == cur->_active_set)
			          cur->_parent->_spawned_set = NULL;
			    cur->_parent->_occupied -= sized_job->size(cur->_parent->_block_size);
			    unlock (cur->_parent, thread_id);
		    }
		    cur->_active_set->_complete = true;
		    release_active_set (cur, thread_id);
	}

	/* Last job done in the system, clean up root */
	Cluster *root = _tree->_root;
	if (job->parent_fork()==NULL && deactivate) {
		std::cout<<"Unpinning the root at thread "<<thread_id<<std::endl;
		if (root->_active_set != NULL) {
		         delete root->_active_set;
			 lock(root, thread_id);
			 root->set_active_set(NULL, thread_id);
			 unlock(root, thread_id);
		}
	}

	print_info (thread_id, sized_job, deactivate);
	release_locks (thread_id);
}

bool
HR_Scheduler::more (int thread_id) {
	std::cerr<<__func__<<" has been deprecated"<<std::endl;
	exit (-1);
}

Job*
HR_Scheduler::find_job (int thread_id, Cluster* node) {
	--_num_jobs;

	/* Go up till an active set is found */
	Cluster * cur;
	for (cur=_tree->_leaf_array[thread_id];
	     cur->_active_set == NULL;
	     cur = cur->_parent);
	
	/* Grab a job from active set, and mark which active set it belongs to */
	if (cur->_active_set->_task_queue.size() == 0) {
	       std::cerr<<"Error: can not call find_job with an empty active set queue"<<std::endl;
	       exit(-1);
	}
	SizedJob * sized_job = cur->_active_set->_task_queue.front();
	assert (sized_job->size(1) > 0);
	sized_job->set_pin_id (cur->_active_set->_parent_job_id);
	cur->_active_set->_task_queue.pop_front();
	
	/* Update allocated size entry on the caches*/
	for (cur = _tree->_leaf_array[thread_id];
	     cur->_active_set == NULL;
	     cur = cur->_parent)
	  cur->_occupied += sized_job->size(cur->_block_size);
	cur->_occupied += sized_job->size(cur->_block_size);

	return sized_job;
}

#define RELEASE_LOCKS_RETURN_NULL(thrid) \
   {release_locks((thrid));		 \
    return NULL; }

#define RELEASE_LOCKS_ASET_RETURN_NULL(cur,thrid) \
   {release_active_set((cur),(thrid));   	  \
    release_locks((thrid));                       \
    return NULL; }

Job*
HR_Scheduler::get (int thread_id) {
	Job* ret;
	Cluster* prev = _tree->_leaf_array[thread_id]; 
	Cluster* cur = prev->_parent;
	lock (prev, thread_id);

	/* If prev has an active set, we handle it right away and return */
	if (prev->_active_set != NULL)  {
		prev->_active_set->lock();
		if (prev->_active_set->_complete) 
 		        RELEASE_LOCKS_ASET_RETURN_NULL (prev, thread_id);
      
		if (prev->_active_set->_task_queue.size() > 0) {
			ret = find_job (thread_id, prev);
			prev->_active_set->unlock();
			unlock (prev, thread_id);      
			return ret;
		} else {
			prev->_active_set->unlock();
			RELEASE_LOCKS_RETURN_NULL(thread_id);
		}
	}

	/* Go up the tree to find an active set */
	/* Invariants: Lock on prev, prev is lowest level, prev->_active_set == NULL */
	do {
	        if (prev->_occupied > prev->_size)  // Heavy strand from above executed along the path
    		        RELEASE_LOCKS_RETURN_NULL(thread_id);
		lock (cur, thread_id);
		if (cur->_occupied > cur->_size)    // H: One of this and above is redundant?
 		        RELEASE_LOCKS_RETURN_NULL(thread_id);
		
		if (cur->_active_set != NULL) {
		       TaskSet *active_set = cur->_active_set;
			active_set->lock();
			
			if (active_set->_complete)
			  RELEASE_LOCKS_ASET_RETURN_NULL(cur, thread_id);

			/* If cur has _spawned_set, set prev's actives_set and use it */
			if (cur->_spawned_set != NULL) {
			        active_set->unlock(); // Don't need it any more
				/* prev and cur are locked. cur->_sp and prev->_act can be modified */
				prev->set_active_set(cur->_spawned_set, thread_id);
				prev->_active_set->change_clusters_attached(1);
				if (prev->_active_set->change_clusters_needed(-1) == 0)
					cur->_spawned_set = NULL;
				unlock (cur, thread_id); 
				active_set = prev->_active_set;
				active_set->lock();
			}
			
                        /* Invariant: active_set is locked, have locks up until node with non-NULL active_set */			
			std::deque<SizedJob*> *active_set_queue = &(active_set->_task_queue);
			if (active_set_queue->size()<=0) {
			        active_set->unlock();
				RELEASE_LOCKS_RETURN_NULL(thread_id);
				/* if (_type==0){Do above} else if (_type == 1) { In alt_HR, search for peer active sets, if one of them has non-empty Q, return it } */
			}

			/* Invariant: sized_job belongs to the lowest active_set,
			 *            no spawned_set at the node corresponding to sized_job */
			
			/* Go up the tree to decide where to pin the sized_job based on it's size */
			SizedJob * sized_job = active_set_queue->at(0);
			if (active_set_queue == &(_tree->_leaf_array[thread_id]->_active_set->_task_queue)) {
			  ret = find_job (thread_id);
				active_set->unlock(); release_locks (thread_id);
				return ret;
			}
			for (prev = _tree->_leaf_array[thread_id], cur = _tree->_leaf_array[thread_id]->_parent;
			     prev->_active_set==NULL; prev = cur, cur = cur->_parent) {
   			        /* Can not put strand on a non-empty cache*/
	  		        if (prev->_occupied > 0) {
  				        active_set->unlock();
					RELEASE_LOCKS_RETURN_NULL(thread_id);
				}
				/* Check if it fits at a lower level. If so, start a new active_set (pin) */				
				lluint size = sized_job->size(cur->_block_size);
				if (size <= cur->_size - cur->_occupied) {
  				        if (size > prev->_size) { //Has to be true due to checks from earlier iterations
					        prev->set_active_set(new TaskSet (sized_job->parent_fork(),sized_job->strand_id(),
										  allocate (size, prev->_size,cur->_size, cur->_num_children)),
								     thread_id);
						prev->_active_set->lock();
						prev->_active_set->_task_queue.push_back (sized_job);
						cur->_occupied += size;
						prev->_active_set->change_clusters_attached(1);
						if (prev->_active_set->change_clusters_needed(-1) > 0) 
						      cur->_spawned_set = prev->_active_set;
						/* H: don't remove this comment: insert Frag 1 */
						active_set_queue->erase(active_set_queue->begin());
						active_set->unlock();
						ret = find_job (thread_id, prev);
						prev->_active_set->unlock();
						release_locks (thread_id);
						return ret;
					} else {
					        std::cerr<<"Error: Control Shouldn'd be here"<<std::endl;
 					        exit(-1); //active_set->unlock();RELEASE_LOCKS_RETURN_NULL(thread_id);
					}
				}     
			}
			/* Doesn't below at a cache below current active_set
			 * Pin it at the level of current active set */
			ret = find_job (thread_id, prev);
			active_set->unlock(); release_locks (thread_id);
			return ret;
		}
		prev = cur; cur = cur->_parent; 
	} while (cur != NULL);
	release_locks (thread_id);
	return NULL; // No job in the system
}

/* DON'T REMOVE: Left out code segments for type 1 */
/* Fragment 1
						if (_type == 1 && cur->_num_children>1) {
						      assert (has_lock (cur, thread_id));
						      cur->_spawned_set_vector.push_back (prev->_active_set);
						      assert (cur->_spawned_set_vector.size () < 10);
                                                      // In Alt_HR, add it to vector sp_set of cur 
					        }
*/
