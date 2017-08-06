
#include <queue>
#include "Thread.hh"
#include <assert.h>
#include <cstdlib>

template <typename T>
class synchronized_queue {
  std::deque<T> _queue;
  Mutex _queuelock;
public:
  synchronized_queue () { };
  ~synchronized_queue() { };

  void push(const T &item) {
    _queuelock.lock();
    _queue.push_back(item);
    _queuelock.unlock();
  }

  void push_front(const T &item) {
    _queuelock.lock();
    _queue.push_front(item);
    _queuelock.unlock();
  }
  
  bool safepop_front(T * ret) {
    /*-- Dangerous? ---*/
    if (_queue.empty())
      return false;
    /*-----------------*/
    _queuelock.lock();
    if (_queue.empty()) {
          _queuelock.unlock();
	  return false;
    }
    *ret = _queue.front();
    _queue.pop_front();
    _queuelock.unlock();
    return true;
  }

  bool safepop_end  (T * ret) {
    /*-- Dangerous? ---*/
    if (_queue.empty())
      return false;
    /*-----------------*/
    _queuelock.lock();
    if (_queue.empty()) {
          _queuelock.unlock();
	  return false;
    }
    *ret = _queue.back();
    _queue.pop_back();
    _queuelock.unlock();
    return true;
  }
  
  T pop_front() {
    _queuelock.lock();
    T t = _queue.front();
    _queue.pop();
    _queuelock.unlock();
    return t;
  }
  
  bool empty() const {return _queue.empty();}
    
  size_t size() const{return _queue.size();}
};

template<typename T>
class DistrQueue {
  int _max_q; 
  synchronized_queue<T> *_queues;
public:
#define DISTRQ_SPACER 2 // To prevent false sharing
  DistrQueue(int max_q) {
    _max_q = max_q;
    _queues = new synchronized_queue<T>[_max_q*DISTRQ_SPACER];
  }

  ~DistrQueue() {
    delete _queues;
  }

  bool check_range (int child_id) {assert (child_id>=0 && child_id<_max_q);}
 
  void reset () {
    for (int i=0; i<_max_q; ++i)
      assert (_queues[i*DISTRQ_SPACER].size() == 0);
  }

  int size (int child_id) {
    _queues[child_id*DISTRQ_SPACER].size();
  }
  
  void add_to_distr_queue (T entry, int child_id) {
    check_range (child_id);
    _queues[child_id*DISTRQ_SPACER].push_front(entry);
  }
  
  bool safeget_from_distr_queue (T *ret, int child_id) {
    check_range(child_id);

    if (_queues[child_id*DISTRQ_SPACER].safepop_front(ret))
      return true;
    // Go in to steal mode
    for (int i=0; i<2*_max_q; ++i) {
      if (_queues[(rand()%_max_q)*DISTRQ_SPACER].safepop_end(ret))
	return true;
    }
    return false;
  }        
};

template<class E>
class Buckets {

public:
  int                          _num_levels;
  lluint                       _size;
  lluint                       _block_size;
  lluint                     * _thresholds; // In decreasing order, the size ceilings for each queue.
                                             // Number of entries = _num_levels
  synchronized_queue<E>**      _queues;      // Classify jobs based on task size, normal queues for bottom buckets
  int                          _num_children;// For distributed queue
  const double                 _sigma;

  Buckets (int num_levels, lluint size, lluint block_size, lluint* thresholds, int num_children, double sigma)
    : _num_levels (num_levels),
      _size (size),
      _block_size (block_size),
      _num_children (num_children),
      _sigma(sigma)
  {
    _thresholds = new lluint [num_levels+1]; _thresholds[num_levels]=0;
    _queues = new synchronized_queue<E>* [num_levels]; 
    for (int i=0; i<num_levels; ++i) {
      _queues[i] = new synchronized_queue<E>;
      _thresholds[i] = thresholds[i];
    }
    _thresholds[0]= 1L << 45;	
    if (_size==0) _size = 1L << 45;
  }
  
  int add_job_to_bucket (E job, int child_id) { // return bucket level
    lluint task_size = job->size(_block_size);
    for (int i=0; i<_num_levels; ++i) {
      if ((double)task_size > _sigma*(double)_thresholds[i+1]) {
	_queues[i]->push_front (job);
	return i;
      }
    }
    assert(false);exit(-1);
  }
  
  int get_job_from_bucket (E* ret, int min_level, int child_id) { // return bucket level
    for (int i=min_level; i<_num_levels; ++i) 
      if (true == _queues[i]->safepop_front(ret)) 
	return i;
    return -1;
  }
  
  void return_to_queue (E job, int level, int child_id) {
    lluint task_size = job->size(_block_size);
    assert ((_sigma*((double)_thresholds[level]))>=(double)task_size
		&& (double)task_size>(_sigma*((double)_thresholds[level+1])));
    _queues[level]->push_front (job);
  }
};

template<class E>
class TopDistrBuckets : public Buckets<E> {
public:
  DistrQueue<E>         * _top_queue;  // Decenrtalized Queue for top bucket
  
  TopDistrBuckets (int num_levels, lluint size, lluint block_size, lluint* thresholds, int num_children, double sigma)
    :  Buckets<E> (num_levels,size,block_size,thresholds,num_children, sigma)  {
    //**** DistrQ for top bucket only to supply level directly below ***
    // **** Don't bother if only 1 or 0 child below ***
    // **** Incremental over H2 ***********
	if (num_children > 1)
	  _top_queue = new DistrQueue<E> (num_children);
	else
	  _top_queue = NULL;
  }
  
  int add_job_to_bucket (E job, int child_id) { // return bucket level
    lluint task_size = job->size(this->_block_size);
    ++this->_num_jobs;
    //***** Incremental over Bucket ********
    if (this->_num_children > 1)
      if ((double)task_size > this->_sigma*this->_thresholds[1]) {
	_top_queue->add_to_distr_queue(job, child_id);
	return 0;
      }
    
    for (int i=0; i<this->_num_levels; ++i) {
      if ((double)task_size > this->_sigma*this->_thresholds[i+1]) {
	this->_queues[i]->push_front (job);
	return i;
      }
    } 
    assert(false);exit(-1);
  }
  
  int get_job_from_bucket (E* ret, int min_level, int child_id) { // return bucket level
    //**** incremental over Bucket *******
    if (this->_num_children>1)
      if (min_level++ ==0)
	if (true == _top_queue->safeget_from_distr_queue(ret,child_id)) {
	  --this->_num_jobs;
	  return 0;
	}
    
    for (int i=min_level; i<this->_num_levels; ++i) {
      if (!this->_queues[i]->empty() && this->_queues[i]->safepop_front(ret) == true) {
	--this->_num_jobs;
	return i;
      }
    }
    return -1;
  }
  
  void return_to_queue (E job, int level, int child_id) {
    lluint task_size = job->size(this->_block_size);
    assert ((this->_sigma*(this->_thresholds[level]))>=(double)task_size
	    && (double)task_size>(this->_sigma*(this->_thresholds[level+1])));
    
    ++this->_num_jobs;
    //**** incremental over Bucket ********
    if (this->_num_children>1 && level==0)
      _top_queue->add_to_distr_queue(job, child_id);
    else
      this->_queues[level]->push_front (job);
  }  
};
