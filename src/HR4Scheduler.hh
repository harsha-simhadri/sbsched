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


#ifndef __HR4SCHEDULER_HH
#define __HR4SCHEDULER_HH

#include "Scheduler.hh"
#include <assert.h>

//#define NDEBUG  // Turn off asserts

#define SIGMA (0.5)
#define MU    (0.2)

class HR4Scheduler : public Scheduler {

typedef struct Cluster {
  friend class TreeOfCaches;
  
  const lluint      _size;                      // In Bytes
  const uint        _block_size;                // In Bytes
  lluint            _occupied;
  int               _num_children;              // No. of subclusters
  int               _sibling_id;
  Cluster *         _parent;
  Cluster **        _children;
  Mutex             _lock;

  /*
  #define INACTIVE_MODE 1
  #define PIN_MODE      2
  #define STRAND_MODE   3
  int               _mode;
  */
  
  Cluster (const lluint size, const int block_size, int num_children, int sibling_id,
	   Cluster * parent=NULL, Cluster ** chidren=NULL);
  
  Cluster *         create_tree ( Cluster * root, Cluster ** leaf_array,
				  int num_levels, uint * fan_outs,
				  lluint * sizes, uint * block_sizes);
  void              lock      () {_lock.lock();}
  void              unlock    () {_lock.unlock();}
  bool              is_locked () {return _lock.is_locked();}

  typedef class Buckets {
  public:
    int                           _num_levels;
    lluint                        _size;
    lluint                        _block_size;
    lluint                      * _thresholds; // In decreasing order, the size ceilings for each queue.
                                            // Number of entries = _num_levels
    synchronized_queue<HR2Job*>** _queues;     // Classify jobs based on task size, normal queues for bottom buckets
    DistrQueue<HR2Job*>        ** _distr_queues;  // Decenrtalized Queue for top bucket
    int                           _num_children; // For distributed queue
    
    Buckets (int num_levels, lluint size, lluint block_size, lluint* thresholds, int num_children)
      : _num_levels (num_levels),
	_size (size),
	_block_size (block_size),
	_num_children (num_children)
      {
	_thresholds = new lluint [num_levels+1]; _thresholds[num_levels]=0;
	_distr_queues=NULL; _queues=NULL;
	
	if (_num_children > 1) 
	  _distr_queues = new DistrQueue<HR2Job*>* [num_levels]; 
	else
	  _queues = new synchronized_queue<HR2Job*>* [num_levels]; 
	
	for (int i=0; i<num_levels; ++i) {
	  _thresholds[i] = thresholds[i];
	  if (_num_children > 1)
	    _distr_queues[i] = new DistrQueue<HR2Job*> (_num_children);
	  else
	    _queues[i] = new synchronized_queue<HR2Job*>;
	}

	_thresholds[0]= 1L << 45;	
	if (_size==0)
	  _size = 1L << 45;
      }
    
    int add_job_to_bucket (HR2Job* job, int child_id) { // return bucket level

      lluint task_size = job->size(_block_size);
      for (int i=0; i<_num_levels; ++i) {
	if (task_size > SIGMA*(double)_thresholds[i+1]) {
	  if (_num_children>1)
	    _distr_queues[i]->add_to_distr_queue(job, child_id);
	  else
	    _queues[i]->push_front (job);
	  return i;
	}
      }

      assert(false);
      exit(-1);
    }
    
    int get_job_from_bucket (HR2Job**ret, int min_level, int child_id) { // return bucket level
      for (int i=min_level; i<_num_levels; ++i) {
	if (_num_children>1) {
	  if (true == _distr_queues[i]->safeget_from_distr_queue(ret,child_id))
	    return i;
	} else {
	  if (true == _queues[i]->safepop_front(ret))
	    return i;
	}
      }
      return -1;
    }
    
    void return_to_queue (HR2Job* job, int level, int child_id) {
      lluint task_size = job->size(_block_size);
      assert ((SIGMA*((double)_thresholds[level]))>=task_size
	      && task_size>(SIGMA*((double)_thresholds[level+1])));
      if (_num_children>1)
	_distr_queues[level]->add_to_distr_queue(job, child_id);
      else
	_queues[level]->push_front (job);
    }
    
  } Buckets;

  Buckets* _buckets;

  /*
  synchronized_queue<Job*>* _queue;

  void add_to_job_queue (Job* job, int child_id) {
    _queue->push_front (job);
  }
  Job* get_from_job_queue (int child_id) {
    Job* ret;
    _queue->safepop_front(&ret);
    return ret;
  }
  void return_to_queue (Job* job, int child_id) {
    _queue->push_front (job);
  }
  */

  /*  
  DistrQueue<Job*> *_distr_queue;
   
  void              add_to_job_queue (Job* job, int child_id) {_distr_queue.add_to_distr_queue (job, child_id);}
  Job*              get_from_job_queue (int child_id)           {return _distr_queue.get_from_distr_queue(child_id);}
  */
  
  // Added by Aapo
  int               _locked_thread_id;         // Thread that locked this cluster
} Cluster;

class TreeOfCaches {
  friend class HR4Scheduler;

  int                 _num_levels;
  int                 _num_leaves;
  
  Cluster *           _root;
  Cluster **          _leaf_array;
  
  int     *           _num_locks_held;           // #locks held by the thread
  Cluster ***         _locked_clusters;          // There is one list for each thread,
                                                 // in the order in which they were obtained.
    
  lluint  *           _sizes;
  uint    *           _block_sizes;
  uint    *           _fan_outs;
};

  
protected:
  
  TreeOfCaches *      _tree;
  
  lluint              _num_jobs;

  int                 _type;                      // 0(def): Spawned set statically alocated to subclusters
                                                  // 1     : Active_set link can move to other spawned set under parent

public:
  HR4Scheduler (int num_threads,                  // Threads are logically numbered left to right.
		int num_levels, int * fan_outs,   // num levels including top level RAM, f_{},
		lluint * sizes, int * block_sizes);// M_{}, B_{}; M_0 is neglected
  ~HR4Scheduler ();
  
   
  void add  (Job *job, int thread_id ); 
  void done (Job *job, int thread_id, bool deactivate );
  Job* get  (int thread_id=-1);
  bool more (int thread_id=-1);
  void print_scheduler_stats() {};

  void pin (HR2Job *job, Cluster *cluster);
  bool fit_job (HR2Job *job, int thread_id, int height, int bucket_level);
  
  void check_lock_consistency (int thread_id);
  
  void lock     (Cluster* node, int thread_id);
  bool has_lock (Cluster* node, int thread_id);
  void unlock   (Cluster* node, int thread_id);
  void print_locks   (int thread_id);
  void release_locks (int thread_id);
  
  void print_tree( Cluster * root, int num_levels, int total_levels=-1);
  void print_job ( HR2Job *job);
};

#endif
