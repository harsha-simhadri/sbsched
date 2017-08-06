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


#ifndef __HR2SCHEDULER_HH
#define __HR2SCHEDULER_HH

#include "Scheduler.hh"
#include <assert.h>

//#define NDEBUG  // Turn off asserts

#define SIGMA (0.5)
#define MU    (0.2)
//#define MU    (0.05)
//#define SIGMA (1.0)
//#define SIGMA (0.5)

class HR2Scheduler : public Scheduler {

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
				  lluint * sizes, uint * block_sizes,
				  int bucket_version);
  void              lock      () {_lock.lock();}
  void              unlock    () {_lock.unlock();}
  bool              is_locked () {return _lock.is_locked();}

  Buckets<HR2Job*>* _buckets;
  
  // Added by Aapo
  int               _locked_thread_id;         // Thread that locked this cluster
} Cluster;

class TreeOfCaches {
  friend class HR2Scheduler;

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

  int                 _type;                      // 0(def): Spawned set statistically alocated to subclusters
                                                  // 1     : Active_set link can move to other spawned set under parent

public:
  HR2Scheduler (int num_threads,                  // Threads are logically numbered left to right.
		int num_levels, int * fan_outs,   // num levels including top level RAM, f_{},
		lluint * sizes, int * block_sizes,// M_{}, B_{}; M_0 is neglected
		int bucket_version);              // 0 for plain Buckets, 1 for TopDistrBuckets
  ~HR2Scheduler ();
  
   
  void add  (Job *job, int thread_id ); 
  void add_multiple  (int num_jobs,Job **jobs, int thread_id ); 
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
