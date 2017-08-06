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


#include "HR4Scheduler.hh"
#include <assert.h>


/* End declarations, functions begin */

HR4Scheduler::Cluster::Cluster (const lluint size, const int block_size,
				int num_children, int sibling_id,
				Cluster * parent, Cluster ** children)
  : _size (size),
    _block_size (block_size),
    _occupied(0),
    _num_children(num_children),
    _sibling_id(sibling_id),
    _parent(parent),
    _locked_thread_id (-1) {
  
	/*_queue = new synchronized_queue<Job*>;
	_distr_task_queue = new ??Q<Job*> [num_children];
	for (int i=0; i<_num_children; ++i)
	_distr_task_queue->add_child(i);*/
	
  _children = new Cluster* [_num_children];
  if (children!=NULL)
    for (int i=0; i<_num_children; ++i)
      _children[i]=children[i];
}

HR4Scheduler::Cluster*
HR4Scheduler::Cluster::create_tree( Cluster * root,
		      Cluster ** leaf_array,
		      int num_levels, uint * fan_outs,
		      lluint * sizes, uint * block_sizes) {
  static int leaf_counter=0;
  
  Cluster * ret;
  if (root == NULL) { // Create the root (RAM) node
    ret = new Cluster (1<<((sizeof(lluint)*4)-1) -1, block_sizes[0],
		       fan_outs[0], 0);
    ret->_buckets = new Buckets (num_levels, 0, *block_sizes, sizes, *fan_outs);
      
    leaf_counter = 0;
    for (int i=0; i<fan_outs[0]; ++i) {
       ret->_children[i] = create_tree (ret, leaf_array,
					num_levels-1, fan_outs+1,
					sizes+1, block_sizes+1);
       ret->_children[i]->_sibling_id = i;
    }
  } else if (num_levels>0) {
    ret = new Cluster (sizes[0], block_sizes[0],
		       fan_outs[0], -1, root);
    ret->_buckets = new Buckets (num_levels, *sizes, *block_sizes, sizes, *fan_outs);
    for (int i=0; i<fan_outs[0]; ++i) {
      ret->_children[i] = create_tree (ret, leaf_array,
				       num_levels-1, fan_outs+1,
				       sizes+1, block_sizes+1);
      ret->_children[i]->_sibling_id = i;
    }
  } else {
    ret = new Cluster (0, 1, 1, -1, root); // L0 cache/register
    
    leaf_array[leaf_counter++] = ret;
  }
  return ret;
}
    
void
HR4Scheduler::print_tree( Cluster * root,
			  int num_levels, int total_levels) {
	if (total_levels == -1) total_levels=num_levels;
	if (num_levels > 0) {
		for (int i=0; i<total_levels-num_levels; ++i)
			std::cout<<"\t\t";
		std::cout<<"Occ:"<<root->_occupied<<" | ";
		if (root->is_locked())
		  std::cout<<"L | ";
		else
		  std::cout<<"U | ";
		for (int i=0; i<root->_buckets->_num_levels; ++i) {
		  if (root->_num_children==1) {
		    std::cout<<root->_buckets->_queues[i]->size()<<",";
		  } else {
		    std::cout<<"(";
		    for (int j=0; j<root->_num_children; ++j)
		      std::cout<<root->_buckets->_distr_queues[i]->size(j)<<",";
		    std::cout<<")";
		  }
		}
		std::cout<<std::endl;
		
		for (int i=0; i<root->_num_children; ++i)
			print_tree (root->_children[i], num_levels-1, total_levels);
	}
}

void
HR4Scheduler::print_job (HR2Job* sized_job) {
	std::cout<<" Job: "<<sized_job->get_id()
	<<", Str: "<<sized_job->strand_id()
	<<", Pin: "<<sized_job->get_pin_id()
	<<", Task_Size: "<<sized_job->size(1)
	<<", Strand_Size: "<<sized_job->strand_size(1)
		 <<std::endl;
}

HR4Scheduler::HR4Scheduler (int num_threads, int num_levels,
			    int * fan_outs, lluint * sizes,
			    int * block_sizes)
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
	
	_tree->_num_locks_held  = new int      [_tree->_num_leaves];
	_tree->_locked_clusters = new Cluster** [_tree->_num_leaves];
	for (int i=0; i<_tree->_num_leaves; ++i) {
		_tree->_num_locks_held[i] = 0;
		_tree->_locked_clusters[i] = new Cluster*[_tree->_num_levels+1];
		for (int j=0; j<_tree->_num_levels+1; ++j)
			_tree->_locked_clusters[i][j] = NULL;
	}
}

HR4Scheduler::~HR4Scheduler() {
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
HR4Scheduler::check_lock_consistency (int thread_id) {
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
HR4Scheduler::lock (Cluster* node, int thread_id) {
  // std::cout<<"Lock, thr: "<<thread_id<<" "<<node<<std::endl;
        if (node->_num_children > 1) {
	      node->lock();
      	      
		assert(node->_locked_thread_id == -1);
		node->_locked_thread_id = thread_id;
	      
	      _tree->_locked_clusters[thread_id][_tree->_num_locks_held[thread_id]] = node;
	      ++_tree->_num_locks_held[thread_id];
	}
}

bool
HR4Scheduler::has_lock (Cluster* node, int thread_id) {
//  std::cerr<<__func__<<": Not currently operational"<<std::endl;
  return (node->_num_children==1 || node->_locked_thread_id == thread_id);
}

void
HR4Scheduler::print_locks (int thread_id) {
	std::cout<<"Thread "<<thread_id<<" has "<<_tree->_num_locks_held[thread_id]
		 <<" locks: ";
	for (int i=0; i<_tree->_num_locks_held[thread_id]; ++i) {
	  std::cout<<" '"<<_tree->_locked_clusters[thread_id][i];
	}
	std::cout<<std::endl;
}


// Check if the node is the last in the list of locked nodes,
void
HR4Scheduler::unlock (Cluster* node, int thread_id) {
  //std::cout<<"Unlock, thr: "<<thread_id<<" "<<node<<std::endl;
  if (node->_num_children > 1) {
	   --_tree->_num_locks_held[thread_id];
	   _tree->_locked_clusters[thread_id][_tree->_num_locks_held[thread_id]] = NULL;
	   
	     assert(node->_locked_thread_id == thread_id);
	     node->_locked_thread_id = -1;
	   
	   node->unlock();
	 }
}

// Release all locks held by thread in the inverse order they were obtained
void
HR4Scheduler::release_locks (int thread_id) {
	while (_tree->_num_locks_held[thread_id] > 0)
		unlock (_tree->_locked_clusters[thread_id][_tree->_num_locks_held[thread_id]-1], thread_id);
}

/* Should be called only by a thread holding a lock on the current cluster */
void
HR4Scheduler::pin (HR2Job *job, Cluster *cluster) {

  assert (cluster->_occupied <= cluster->_size);

  job->pin_to_cluster (cluster, job->size(cluster->_block_size));
}

void
HR4Scheduler::add (Job* uncast_job, int thread_id) {
	HR2Job* job = (HR2Job*)uncast_job;
	Cluster *root = _tree->_root;

	/* Job added by an agent other than the threads */
	if (thread_id == _num_threads) {
	        root->lock ();
		pin (job, root);
		root->_occupied+=job->size(root->_block_size);
		root->_buckets->add_job_to_bucket (job, 0);
		root->unlock ();
		return;
	}
	
	/* If root has no active job, set it here */
	if (job->get_pin_cluster() == NULL) {
		for (Cluster *cur = _tree->_leaf_array[thread_id];cur->_parent!=NULL;cur = cur->_parent)
			lock (cur, thread_id);

		pin (job, root);
		root->_occupied+=job->size(root->_block_size);
		release_locks (thread_id);
	}

	/* Add job to the approporiate queue */
	int child_id=0;
	for (Cluster * cur = _tree->_leaf_array[thread_id];;cur = cur->_parent ) {
		if (job->get_pin_cluster() == cur)  {
		  cur->_buckets->add_job_to_bucket (job, child_id);
			return;
		}
		child_id = cur->_sibling_id;
	}

	assert (false);exit(-1);
}

void
HR4Scheduler::done (Job *uncast_job, int thread_id, bool deactivate) {
 
        HR2Job  * job = (HR2Job*)uncast_job;
	Cluster * cur  = _tree->_leaf_array[thread_id];
	Cluster * pin = (Cluster*) job->get_pin_cluster();

	/* Update occupied size */
	for ( ; cur!=pin; cur=cur->_parent) {
	  lluint strand_size = job->strand_size(cur->_block_size);
	  cur->_occupied -= strand_size>(int)(MU*(cur->_size))
	                    ? (int)(MU*(cur->_size))
	                    : strand_size;
	}
	
	/* If the done task started a pin, clean up the allocation */
	if (deactivate) {  // Strand joins and end its task
	  if (job->is_maximal()) {
	    lock (cur, thread_id);
	    cur->_occupied -= job->size(cur->_block_size);
	  }
	} 	  
	release_locks (thread_id);

	/* Last job done in the system */
	if (job->parent_fork()==NULL && deactivate)
		std::cout<<"Finished root task at thread: "<<thread_id<<std::endl;
}

bool
HR4Scheduler::fit_job (HR2Job *job, int thread_id, int height, int bucket_level) {
  Cluster *leaf=_tree->_leaf_array[thread_id];
  Cluster *cur=leaf;

  for (int i=0; i<height-bucket_level; ++i) {
    lock (cur, thread_id);
    if (cur->_occupied > (1-MU)*(double)cur->_size) {
      release_locks (thread_id);
      return false;
    }    
    cur=cur->_parent;
  }

  lluint task_size = job->size (cur->_block_size);
  assert (task_size <= SIGMA*(cur->_size) || cur->_parent == NULL);

  if (bucket_level > 0) {
    lock (cur, thread_id);
    if (task_size > cur->_size-cur->_occupied) {
      release_locks (thread_id);
      return false;
    } else {
      pin (job, cur);
      assert (job->is_maximal());
      cur->_occupied += task_size;
    }
  } else {
    assert (job->get_pin_cluster() == cur);
  }
    
  for (Cluster *iter=leaf; iter!=cur ; iter=iter->_parent) {
    lluint strand_size = ((HR2Job*)job)->strand_size (iter->_block_size);
    assert (has_lock (iter, thread_id));
    assert (iter->_occupied <= (1-MU)*iter->_size);
    iter->_occupied += strand_size<(int)(MU*iter->_size) ? strand_size : (int)(MU*iter->_size);
  }

  release_locks(thread_id);
  return true;
}

Job*
HR4Scheduler::get (int thread_id) {
   	HR2Job * job = NULL;
   	int height=1; int child_id=_tree->_leaf_array[thread_id]->_sibling_id;
     
	for ( Cluster *cur=_tree->_leaf_array[thread_id]->_parent;
	      cur!=NULL;
	      cur=cur->_parent,++height) {
	  int level = cur->_buckets->get_job_from_bucket(&job, 0, child_id);
	  while (level !=-1) {
	    if (fit_job (job, thread_id, height, level) == true) {
	      release_locks(thread_id);
	      return job;
	    } else {
	      cur->_buckets->return_to_queue (job, level, child_id);
	    }
	    level = cur->_buckets->get_job_from_bucket(&job, 1+level, child_id);	    
	  } 
	  
	  if ( cur->_occupied > (int)((1-MU)* cur->_size) ) 
	    return NULL;
	  
	  child_id = cur->_sibling_id;
	}
	return NULL;
}

bool
HR4Scheduler::more (int thread_id) {
	std::cerr<<__func__<<" has been deprecated"<<std::endl;
	exit (-1);
}

