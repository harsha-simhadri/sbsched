#include "ThreadPool.hh"
#include "sequence-jobs.hh"
//#include "getperf.hh"

class CheckProc : public SizedJob {
public:
  CheckProc (bool del=true)
    : SizedJob (del)  {}
  
  lluint size (const int block_size) {return 2;}
  
  void function () {
    int sum=5;
    //for (int i=0; i<1<<25; ++i) sum+=5;
    std::cout<<"Job on processor: "<<sched_getcpu()<<" Nonsense value: "<<sum<<std::endl;
    join ();
  }
};

// Return time in microsecs
lluint
check_processor_pair (uint p1, uint p2, ThreadPool *thread_pool, Job *job) {
  if (job==NULL) {
    std::cerr<<"Null job passed to check processor pair affinity"<<std::endl;
    exit(-1);
  }
  
  lluint startT = get_time_microsec ();
  //uint64_t startCounter = readCounter ();
  
  for (int i=0; i<8; ++i) {
    thread_pool->set_null_join ();
    thread_pool->add_job( job, p1 );
    thread_pool->sync_all();
    
    thread_pool->set_null_join ();
    thread_pool->add_job( job, p2 );
    thread_pool->sync_all();
  }
  lluint duration = get_time_microsec()-startT;
  std::cout<<duration/1000;//<<", "<<(readCounter()-startCounter)/1000<<"K>";
  return duration;
}

void
print_affinity (int num_procs, uint* map) {
  ThreadPool *thread_pool;
  uint *linear_map = new uint[num_procs];
  for (int i=0; i<num_procs; ++i)
    linear_map[i] = i;
  if ((thread_pool = new ThreadPool( num_procs, new Local_Scheduler(num_procs), linear_map)) == NULL)
    std::cerr << "(init_thread_pool) could not allocate thread pool" << std::endl;

  int LEN=(1<<18);
  volatile double * volatile data = new double[8*LEN];
  volatile double * volatile storage = new double[8*LEN];

  volatile double*  *A = new volatile double*[LEN];
  volatile double*  *B = new volatile double*[LEN];

  for (int i=0; i<LEN; ++i) {
    data[8*i] = (34634.0*rand())/RAND_MAX;
    A[i] = data + 8*i;
    B[i] = storage + 8*(LEN-i-1);
  }
  std::random_shuffle (A, A+LEN);
  std::random_shuffle (B, B+LEN);

	       
  //Job* job = new GatherScatter<volatile double> (A,B,LEN,false);
  Job* job = new IncrementArrayRef<volatile double> (A,LEN,false);
  //Job* job = new Scatter<volatile double> (B,LEN,false);
  //Job* job = new Gather<volatile double> (A,LEN,false);
  start_timers (num_procs);
  //startCounter ( LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES, -1);
  
  for (int i=0; i<num_procs; ++i) {
    for (int j=0; j<num_procs; ++j) {
      std::cout<<" ";
      check_processor_pair (map[i], map[j],thread_pool,job);
      //check_processor_pair (i, j,thread_pool,job);
    }
    std::cout<< "\n ";
  }

  std::cout<<"Junk: "<<A[346]<<" "<<*B[32]<<std::endl;
  //print_timers (num_procs);
  delete thread_pool;
}

void
affinity_sanity_check (int num_procs) {
  
  ThreadPool *thread_pool;
  uint *linear_map = new uint[num_procs];
  for (int i=0; i<num_procs; ++i)
    linear_map[i] = i;
  if ((thread_pool = new ThreadPool( num_procs, new Local_Scheduler(num_procs), linear_map)) == NULL)
    std::cerr << "(init_thread_pool) could not allocate thread pool" << std::endl;
	       
  Job* job = new CheckProc (false);
 
  for (int i=0; i<num_procs; ++i) {
    std::cout<<"Pushing to "<<i<<", ";
    //thread_pool->set_thread_affinity (0, i);
    thread_pool->set_null_join();
    thread_pool->add_job (job, i);
    thread_pool->sync_all ();
  }

  delete thread_pool;
}
