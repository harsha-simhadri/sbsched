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


#ifndef __HR1SCHEDULER_HH
#define __HR1SCHEDULER_HH

#include "Scheduler.hh"

class HR_Scheduler : public Scheduler {
  
  typedef struct TaskSet {
    friend class HR_Scheduler;
    
    volatile int               _clusters_needed;
    volatile int               _clusters_attached;
    
    bool              _complete;                   // Is this task complete?
    
    lluint            _parent_job_id;
    Fork *            _parent_fork;
    Mutex             _lock;
    
    std::deque<SizedJob*> _task_queue;
    
    TaskSet (Fork * parent_fork, lluint parent_job_id, int num)
      : _parent_fork (parent_fork),
	_parent_job_id (parent_job_id),
	_clusters_needed (num),
	_clusters_attached (0),
	_complete (false)
    {}
    
    int               change_clusters_needed(int delta) {
      return __sync_add_and_fetch(&_clusters_needed, delta); 
    }
    int               change_clusters_attached(int delta) {
      return __sync_add_and_fetch(&_clusters_attached, delta); 
    }
    void              lock   ()   {_lock.lock();}
    void              unlock ()   {_lock.unlock();}
  } TaskSet;

  struct Cluster;  
  typedef struct Cluster {
    friend class TreeOfCaches;
    
    const lluint      _size;                      // In Bytes
    const uint        _block_size;                // In Bytes
    int               _num_children;              // No. of subclusters
    Cluster *         _parent;
    Cluster **        _children;
    
    
    
    lluint            _occupied;                  // Occupied size in byes
    
    TaskSet *         _active_set;                // A cluster can execute tasks only from this set
    
    TaskSet *         _spawned_set;               // Set of tasks pinned to a cluster
                                                // In other words, tasks can be executed only under this cluster
                                                // Sublcusters point to this set using their _active_set pointers.
  
    Mutex             _lock;
    
    Cluster (const lluint size, const int block_size,
	     int num_children, Cluster * parent=NULL, Cluster ** children=NULL);
    
    Cluster *         create_tree ( Cluster * root, Cluster ** leaf_array,
				    int num_levels, uint * fan_outs,
				    lluint * sizes, uint * block_sizes);
    void              lock      () {_lock.lock();}
    void              unlock    () {_lock.unlock();}
    bool              is_locked () {return _lock.is_locked();}
    
    std::vector<TaskSet*> _spawned_set_vector;      // List of spawned sets that are currently active on subclusters
    // Need lock on cluster to access
    
    // Added by Aapo
    int               _locked_thread_id;         // Thread that locked this cluster
    void set_active_set(TaskSet * a, int thrid) { 
      assert(thrid == _locked_thread_id);
      _active_set = a; 
    }
    
    
  } Cluster;
  
  class TreeOfCaches {
    friend class HR_Scheduler;
    
    int                 _num_levels;
    int                 _num_leaves;
    
    Cluster *           _root;
    Cluster **          _leaf_array;
    
    int     *           _num_locks_held;           // #locks held by the thread
    Cluster ***         _locked_clusters;         // There is one list for each thread,
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

  Cluster *           find_active_set (int thread_id, SizedJob* sized_job);
                                                  // Search from the leaves of the tree up until you
                                                  // find an active set to which sized_job is pinned
                                                  // and return it. Lock up all nodes till there.

  void                release_active_set (Cluster *node, int thrid);
                                                  // Require that node's active_set be locked,
                                                  // and that the active_set be completed.

  void                insert_task_in_queue (std::deque<SizedJob*> &task_queue, SizedJob *job, lluint block_size);
                                                  // Insert task in to the task queue of an active set
  
public:
  HR_Scheduler (int num_threads,                  // Threads are logically numbered left to right.
		int num_levels, int * fan_outs,   // num levels including top level RAM, f_{},
		lluint * sizes, int * block_sizes,// M_{}, B_{}; M_0 is neglected
		int type=0);
  ~HR_Scheduler ();
  
  int allocate (lluint size,
		lluint lower_cache_size,          // Set this to 0, if this is processor and upper is L1
		lluint upper_cache_size,
		int fan_out) {
    #define MIN(A, B) ((A<B)?A:B)
    return (lower_cache_size == 0
	    ? fan_out
	    : MIN (fan_out, (int)ceil (((double)size)/((double)lower_cache_size))));
    //: (int)ceil (fan_out*(((double)size)/((double)upper_cache_size))));
  }
  
 
  void add  (Job *job, int thread_id ); 
  void done (Job *job, int thread_id, bool deactivate );
  Job* get  (int thread_id=-1);
  bool more (int thread_id=-1);
  void print_scheduler_stats() {};
  Job* find_job (int thread_id, Cluster *node=NULL);

  void check_lock_consistency (int thread_id);
  
  void lock     (Cluster* node, int thread_id);
  bool has_lock (Cluster* node, int thread_id);
  void unlock   (Cluster* node, int thread_id);
  void print_locks   (int thread_id);
  void release_locks (int thread_id);

  Mutex _print_lock;
  void print_tree( Cluster * root, int num_levels, int total_levels=-1);
  void print_job ( SizedJob *job);
  void print_info (int thread_id, SizedJob* sized_job, int deactivate);
};

#endif
