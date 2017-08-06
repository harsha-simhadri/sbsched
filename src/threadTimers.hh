#ifndef __TIMERS_HH
#define __TIMERS_HH

typedef unsigned int  uint;

#define BUFFER 64
#define MAX_TIMERS 8
#define loc(i,j) (i*BUFFER + j)

class ThreadTimer {
protected:
  bool active;
  long long int * timers;
  uint num_procs;
public:
  ThreadTimer (uint p) {
    active =false;
    timers = new long long int[p*BUFFER];
    for (int i=0; i<p; ++i)
      for (int j=0; j<MAX_TIMERS; ++j)
	timers[loc(i,j)] = 0;
    num_procs = p;
  }
  ~ThreadTimer () {delete timers;}

  void inline add (uint proc_id, int timer_id, long long int time) {if(active) timers[loc(proc_id,timer_id)] += time;}
  void inline subtract (uint proc_id, int timer_id, long int val) {/*if(active) */timers[loc(proc_id,timer_id)] -= val;}
  inline long long int operator() (uint proc_id, int timer_id)     {return timers[loc(proc_id,timer_id)];}
  inline long long int get (uint proc_id, int timer_id)     {return timers[loc(proc_id,timer_id)];}  
  inline void activate() {active=true;}
  inline void deactivate() {active=false;}

  long int avg (int timer_id) {
    long int sum=0;
    for (int i=0; i<num_procs; ++i)
      sum += timers[loc(i,timer_id)];
    return sum/num_procs;
  }

  void reset (int timer_id, int proc_id=-1) {
    if (proc_id == -1)
      for (int i=0; i<num_procs; ++i)
	timers[loc(i,timer_id)] = 0;
    else
      timers[loc(proc_id,timer_id)] = 0;
  }
};

class ThreadCounter : public ThreadTimer {
public:
  ThreadCounter (uint p) :
    ThreadTimer(p) {}
  
  void inline increment (uint proc_id, int counter_id) {if(active) ++timers[loc(proc_id,counter_id)];}  
};
  /*
long long int * counters;
  uint num_procs;
public:
  ThreadCounter (uint p) {
    counters = new long long int [p*BUFFER];
    for (int i=0; i<p; ++i)
      for (int j=0; j<MAX_TIMERS; ++j)
	counters[loc(i,j)] = 0;
    num_procs = p;
  }
  ~ThreadCounter () {delete counters;}

  void inline add (uint proc_id, int counter_id, long int val) {counters[loc(proc_id,counter_id)] += val;}
  
  long long int inline operator() (uint proc_id, int counter_id) {return counters[loc(proc_id,counter_id)];}
  
  long int avg (int counter_id) {
    long int sum=0;
    for (int i=0; i<num_procs; ++i)
      for (int j=0; j<counter_id; ++j)
	sum += counters[loc(i,j)];
    return sum/num_procs;
  }

  void reset (int counter_id, int proc_id=-1) {
    if (proc_id == -1)
      for (int i=0; i<num_procs; ++i)
	counters[loc(i,counter_id)] = 0;
    else
      counters[loc(proc_id,counter_id)] = 0;
      }*/


/*
class PerfCounters {
#define MAX_PERF_COUNTERS 4
#define MAX_GLOBAL_PERF_COUNTERS 4

  uint        num_procs;
  uint        max_counters;
  uint        max_global_counters;

  long int ** perf;                 // perf[i][j] for counter i on proc j
  string   *  counter_name;         // Name of the counters
  int         num_counters;         // Number of counters initialized
  
  long int *  global_perf;          // global_perf[i] for counter i
  string   *  global_counter_name;  // Name for global  counter i
  int         num_global_counters;  // Number of global couners initialized
  
public:

  PerfCounters (uint num_procs_,
		uint max_counters_=-1,
		uint max_global_counters_=-1) {
    num_procs           = num_procs_;
    num_counters        = 0;
    num_global_counters = 0;
    max_counters        = max_counters_ == -1
                        ? MAX_PERF_COUNTERS : max_counters_ ;
    max_global_counters = max_global_counters_ == -1
                        ? MAX_GLOBAL_PERF_COUNTERS : max_global_perf_counters_ ;
    
    perf                = new lont int *[max_counters];
    global_perf         = new long int [max_global_counters];
    counter_name        = new string[max_counters];
    global_counter_name = new string[max_counters];    
  }

  // Counter disabled at init /
  init_counter (uint counter, string counter_name, int proc=-1) {

  }

  enable_counter (uint counter, int proc) {
  }

  disable_counter (uint counter, int proc) {
  }

  init_global_counter (uint counter, stirng counter_name) {
    if (num_global_counters == max_global_counters) {
      std::cerr<<"Ran out of global counters"<<std::endl; exit(-1);
    }
    ++num_global_counters;
    global_perf[num_global_counters] = new_counter (counter, -1, -1);
    global_counter_name[num_global_counters] = 
  }

  enable_global_counter (uint counter) {
  }

  disable_global_counter (uint counter) {
  }
};
*/

#endif
