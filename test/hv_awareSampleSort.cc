#include <stdlib.h>
#include "ThreadPool.hh"
#include "machine-config.hh"
#include "sequence-jobs.hh"
#include "quickSort.hh"
#include "parse-args.hh"
using namespace std;

template <class ET, class F> 
int search(ET* S, int n, ET v, F f) {
  for (int i=0; i < n; i++)
    if (f(v,S[i])) return i;
  return n;
}

template <class ET, class F> 
int binSearch(ET* S, int n, ET v, F f) {
  ET* T = S;
  while (n > 10) {
    int mid = n/2;
    if (f(v,T[mid])) n = mid;
    else {
      n = (n-mid)-1;
      T = T + mid + 1;
    }
  }
  return T-S+search(T,n,v,f);
}

// template <class E, class BinPred>
// void insertionSort(E* A, int n, BinPred f) {
//   for (int i=0; i < n; i++) {
//     E v = A[i];
//     E* B = A + i;
//     while (--B >= A && f(v,*B)) *(B+1) = *B;
//     *(B+1) = v;
//   }
// }

template <class E, class BinPred>
E median(E a, E b, E c, BinPred f) {
  return  f(a,b) ? (f(b,c) ? b : (f(a,c) ? c : a)) 
    : (f(a,c) ? a : (f(b,c) ? c : b));
}

template <class E, class BinPred>
pair<E*,E*> split(E* A, int n, BinPred f) {
  E p = median(A[n/4],A[n/2],A[(3*n)/4],f);
  E* L = A;   // below L are less than pivot
  E* M = A;   // between L and M are equal to pivot
  E* R = A+n-1; // above R are greater than pivot
  while (1) {
    while (!f(p,*M)) {
      if (f(*M,p)) swap(*M,*(L++));
      if (M >= R) break; 
      M++;
    }
    while (f(p,*R)) R--;
    if (M >= R) break; 
    swap(*M,*R--); 
    if (f(*M,p)) swap(*M,*(L++));
    M++;
  }
  return pair<E*,E*>(L,M);
}

//#define ISORT 25
#define SSORT 50000

template <class E, class BinPred>
void quickSortSerial(E* A, int n, BinPred f) {
  if (n < ISORT) insertionSort(A, n, f);
  else {
    pair<E*,E*> X = split(A,n,f);
    quickSortSerial(A, X.first - A, f);
    // Exclude all elts that equal pivot
    quickSortSerial(X.second, A+n-X.second, f); 
  }
}

template <class E, class BinPred>
class quickSortP : public HR2Job {
  E* A; int n;
  BinPred f;
  int stage;

public:
  quickSortP(E* A_, int n_, BinPred f_, int stage_=0,
       bool del=true)
    : HR2Job (del), A(A_), n(n_), f(f_), stage(stage_)  {}

  lluint size (const int block_size) { 
    return (n*sizeof(E)/2);}
  lluint strand_size (const int block_size) { 
    return size(block_size);}
  
  void function () {
    if (stage == 0) {
      if (n < SSORT) {
	seqQuickSort(A, n, f);
	join();
      } else {
	pair<E*,E*> X = split(A,n,f);
	binary_fork (new quickSortP(A, X.first -A, f),
		     new quickSortP(X.second, A+n-X.second,f),
		     new quickSortP(A,n,f,1));
      }
    } else {
      join();
    }
  }
};

template <class ftype>
class mapIndex : public HR2Job {
  int s; int n; 
  long isize;
  ftype f;
  int stage;

public:
  mapIndex(int s_, int n_, long isize_, ftype f_, int stage_=0,
       bool del=true)
    : HR2Job (del), n(n_), s(s_), isize(isize_), f(f_), stage(stage_)  {}

  lluint size (const int block_size) { 
    //if (block_size!=64) printf("bsize=%d,%d\n",block_size,n);
    //printf("bsize=%d,%d\n",block_size,n);
    //int my_block_size = 64;
    if (n==1) return 2000000;
    else return n * isize;
  }
  lluint strand_size (const int block_size) { 
    return 2000000;} //size(block_size);}
  
  void function () {
    if (stage == 0) {
      if (n==1) {
	f(s);
	join ();
      } else {
	binary_fork (new mapIndex(s,n/2,isize,f),
		     new mapIndex(s+n/2,n-n/2,isize,f),
		     new mapIndex(s,n,isize,f,1));
      }
    } else {
      join ();
    }
  }
};

long ts;

template <class E, class BinPred>
class genCounts {
  E *A;
  int *bucketId;
  int *counts;
  E *pivots;
  int n, numBuckets, numBlocks;
  BinPred f;

public:
  genCounts(E *A_, int* bucketId_,  int *counts_, E* pivots_, 
	    int n_, int numBuckets_, int numBlocks_, BinPred f_) :
    A(A_), bucketId(bucketId_), counts(counts_), pivots(pivots_), 
    n(n_), numBuckets(numBuckets_), numBlocks(numBlocks_), f(f_)
  {}

  void operator() (int i) {
    //int td = example_get_time() -ts;
    //printf("i=%d: %d\n", i, td);

    int blockSize = (n-1)/numBlocks + 1;
    int offset = i * blockSize;
    int size =  (i < numBlocks - 1) ? blockSize : n - offset; 
    //int *mycounts = counts + i * numBuckets;
    int *mycounts = newA(int,numBuckets+64);
    for (int j = 0; j < numBuckets; j++) 
      mycounts[j] = 0;
    for (int j = offset; j < offset + size; j++) {
      int bucket = binSearch(pivots, numBuckets-1,A[j],f);
      bucketId[j] = bucket;
      mycounts[bucket]++;
    }
    for (int j = 0; j < numBuckets; j++) 
      counts[j + i*numBuckets] = mycounts[j];
    free(mycounts);
    return;
  }
};

template <class E>
class relocate {
  E *A, *B;
  int *bucketId;
  int *counts;
  int n, numBuckets, numBlocks;

public:
  relocate(E *A_, E *B_, int* bucketId_,  int *counts_,
	   int n_, int numBuckets_, int numBlocks_) :
    A(A_), B(B_), bucketId(bucketId_), counts(counts_), 
    n(n_), numBuckets(numBuckets_), numBlocks(numBlocks_)
  {}

  void operator() (int i) {
    int blockSize = (n-1)/numBlocks + 1;
    int offset = i * blockSize;
    int size =  (i < numBlocks - 1) ? blockSize : n - offset; 
    //int *mycounts = counts + i * numBuckets;
    int *mycounts = newA(int,numBuckets+64);
    for (int j = 0; j < numBuckets; j++) 
      mycounts[j] = counts[j + i*numBuckets];
    for (int j = offset; j < offset + size; j++) 
      B[mycounts[bucketId[j]]++] = A[j];
    free(mycounts);
  }
};

template <class E, class BinPred>
class qsortBlock {
  E *A;
  int *bucketStarts;
  BinPred f;

public:
  qsortBlock(E *A_, int *bucketStarts_, BinPred f_) :
  A(A_), bucketStarts(bucketStarts_), f(f_) {}

  void operator() (int i) {
    int size = bucketStarts[i+1] - bucketStarts[i];
    //printf("%d: %d, %d\n", i, bucketStarts[i], size);
    //int td = example_get_time() -ts;
    //printf("i=%d: %d\n", i, td);
    seqQuickSort<E,BinPred>(A + bucketStarts[i], size, f);
  }
};

template <class E, class BinPred>
class mapQuicksort : public HR2Job {
  E* A;
  int *bucketStarts;
  int s; int n; 
  BinPred f;
  int stage;

  E* B_space;
  int *compared_space;
  int *less_pos_space;
  int *more_pos_space;

public:
  mapQuicksort(E *A_, int *bucketStarts_, int s_, int n_, 
	       BinPred f_, int stage_=0, E* B_space_=NULL, int* compared_space_=NULL,
	       int *less_pos_space_=NULL, int *more_pos_space_=NULL, 
	       bool del=true)
    : HR2Job (del), A(A_), bucketStarts(bucketStarts_), s(s_), n(n_), f(f_), 
      B_space(B_space_), compared_space(compared_space_),
      less_pos_space(less_pos_space_), more_pos_space(more_pos_space_),
      stage(stage_)  {}

  lluint size (const int block_size) { 
    return ((long) 400000 * 28)*n;}
  lluint strand_size (const int block_size) { 
    return size(block_size);}
  
  void function () {
    if (stage == 0) {
      if (n==1) {
	//int td = example_get_time() -ts;
	//printf("i=%d: %d\n", s, td);	
	int size = bucketStarts[s+1] - bucketStarts[s];
	if (1) {
	  unary_fork(new QuickSort<E,BinPred>(A+bucketStarts[s],size,f,
					      compared_space,less_pos_space,more_pos_space,B_space),
		     new mapQuicksort(A,bucketStarts,s,n,f,1));
	} else {
	  unary_fork(new quickSortP<E,BinPred>(A+bucketStarts[s],size,f),
		     new mapQuicksort(A,bucketStarts,s,n,f,1));
	}
      } else {
	int size = bucketStarts[s+n] - bucketStarts[s];
	int half_size =bucketStarts[s+n/2] - bucketStarts[s];

	  binary_fork (new mapQuicksort(A,bucketStarts,s,n/2,f,0,B_space,compared_space,less_pos_space,more_pos_space),
		       new mapQuicksort(A,bucketStarts,s+n/2,n-n/2,f,0,B_space+half_size,compared_space+half_size,less_pos_space+half_size,more_pos_space+half_size),
		       new mapQuicksort(A,bucketStarts,s,n,f,1,B_space,compared_space,less_pos_space,more_pos_space,true));
      }
    } else {
      //int td = example_get_time() -ts;
      //printf("end: i=%d: %d\n", s, td);
      join ();
    }
  }
};

template <class E, class BinPred>
class SampleSort : public HR2Job {
  E *A, *B; 
  int n;
  BinPred f; 
  int numBlocks, numBuckets;
  int *counts;
  int *bucketId;
  int *bucketStarts;
  int stage;
  
  E *B_space;
  int *compared_space;
  int *less_pos_space;
  int *more_pos_space;
  
public:
  SampleSort (E* A_, E* B_, int* counts_, int* bucketId_,
	      int numBlocks_, int numBuckets_, int n_, 
	      BinPred f_, 
	      E* B_space_=NULL, int* compared_space_=NULL, int* less_pos_space_=NULL, int* more_pos_space_=NULL,
	      bool del=true)
    : HR2Job (del), A(A_), B(B_), counts(counts_), bucketId(bucketId_),
      numBlocks(numBlocks_), numBuckets(numBuckets_), n(n_), f(f_), 
      B_space(B_space_), compared_space(compared_space_), less_pos_space(less_pos_space_), more_pos_space(more_pos_space_),
      stage(0) {}

  SampleSort (SampleSort *from, bool del=true)
    : HR2Job (del), 
      A(from->A), B(from->B), 
      n(from->n), 
      f(from->f), 
      numBlocks(from->numBlocks),
      numBuckets(from->numBuckets), 
      counts(from->counts),
      bucketId(from->bucketId),
      bucketStarts(from->bucketStarts),
      B_space(from->B_space),
      compared_space(from->compared_space),
      less_pos_space(from->less_pos_space),
      more_pos_space(from->more_pos_space),      
      stage(from->stage+1) 
  {}

  lluint size (const int block_size) { 
    return 100000000; }

  lluint strand_size (const int block_size) { 
    return size(block_size); }
  
  void function () {
    if (stage == 0) {
      nextTime("Start");

      //numBlocks = 32;
      //int bucketSize = 100000;
      int overSample = 40;
      //numBuckets = (n-1)/bucketSize + 1;
      int sampleSetSize = numBuckets*overSample;
      E* sampleSet = newA(E,sampleSetSize);

      // generate samples with oversampling
      for (int j=0; j< sampleSetSize; ++j) {
	int o = utils::hash(j)%n;
	sampleSet[j] = A[o]; 
      }

      // sort the samples
      seqQuickSort(sampleSet, sampleSetSize, f);

      // subselect samples at even stride
      E* pivots = newA(E,numBuckets-1);
      for (int k=1; k < numBuckets; ++k) {
	int o = overSample*k;
	pivots[k-1] = sampleSet[o];
      }
      free(sampleSet);  
      nextTime("Finding pivots");
      //cout << "numBuckets = " << numBuckets << endl;
      //cout << "numBlocks = " << numBlocks << endl;

      //counts = newA(int, numBlocks*numBuckets);
      //bucketId = newA(int,n);

      ts = example_get_time();     
      int isize = n*(sizeof(double)+sizeof(int))/numBlocks;
      unary_fork (new mapIndex<genCounts<E,BinPred> >(0, numBlocks, isize, genCounts<E,BinPred>(A, bucketId, counts, pivots, n, numBuckets, numBlocks, f)),
		  new SampleSort(this));

    } else if (stage == 1) {

      nextTime("Counting Buckets");
      int sum = 0;
      for (int i = 0; i < numBuckets; i++)
	for (int j = 0; j < numBlocks; j++) {
	  int v = counts[j*numBuckets+i];
	  counts[j*numBuckets+i] = sum;
	  sum += v;
	}

      bucketStarts = newA(int,numBuckets+1);
      for (int i = 0; i < numBuckets; i++)
	bucketStarts[i] = counts[i];
      bucketStarts[numBuckets] = n;

      nextTime("Bucket offsets");
      //B = newA(E, n);

      int isize = n*(2*sizeof(double)+sizeof(int))/numBlocks;
      unary_fork (new mapIndex<relocate<E> >(0, numBlocks, isize,
            relocate<E>(A, B, bucketId, counts, n, numBuckets, numBlocks)),
		  new SampleSort(this));

    } else if (stage == 2) {

      nextTime("Relocating");
      ts = example_get_time();     
      unary_fork(
		 //new mapIndex<qsortBlock<E,BinPred> >(0, numBuckets, qsortBlock<E,BinPred>(B, bucketStarts, f)),
		 new mapQuicksort<E,BinPred>(B,bucketStarts,0,numBuckets,f,0,B_space,compared_space,less_pos_space,more_pos_space),
		 new SampleSort(this));

    } else if (stage == 3) {

      nextTime("Sorting Blocks");
      free(bucketStarts);
      join();

    }
  }
};

int main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? 100000000 : get_size(argv, argc,2);

  int numBlocks = 32; //128;
  int bucketSize = 400000; //1000000;
  int numBuckets = (LEN-1)/bucketSize + 1;
  double *A, *B;
  int *counts, *bucketId;

  A = new double[LEN];
  B = new double[LEN];
  bucketId = new int[LEN];
  counts = new int[numBlocks*numBuckets];
  
  //_Pragma("omp parallel for")
  for (int i=0; i<LEN; ++i) {
    A[i] = utils::hash(i);   
	B[i] = 0;
	bucketId[i] = 0;
  }
  
  //_Pragma("omp parallel for")
  for (int i=0; i<numBuckets*numBlocks; ++i) {
    counts[i] = 0;
  }

  double* B_space = (double*) newA(double,LEN+1);
  int* compared_space = (int*) newA(int,LEN+1);
  int* less_pos_space = (int*) newA(int,LEN+1);
  int* more_pos_space = (int *) newA(int,LEN+1);

  stripe((char*)B_space,sizeof(double)*(LEN+1),1<<12);
  stripe((char*)compared_space,sizeof(int)*(LEN+1),1<<12);
  stripe((char*)less_pos_space,sizeof(int)*(LEN+1),1<<12);
  stripe((char*)more_pos_space,sizeof(int)*(LEN+1),1<<12);

  FIND_MACHINE;
  Scheduler *sched=create_scheduler (argv, argc);
  flush_cache(num_procs,sizes[1]);  
  cout<<"Len: "<<LEN<<endl;
  startTime();
  tp_init (num_procs, map, sched,
	   new SampleSort<double,less<double> >(A, B, counts, bucketId, 
						numBlocks, numBuckets, LEN, 
						less<double>(),
						B_space,compared_space,less_pos_space,more_pos_space));
  tp_sync_all ();
  nextTime("Total time, measured from driver program");

  for (int i=0; i < 20; i++) 
    //cout << B[i] << endl;

  for (int i=1; i < LEN; i++) {
    if (B[i] < B[i-1]) {
      cout << "not sorted at position: " << i << 
	" (" << B[i-1] << "," << B[i] << ")" <<endl;
      abort();
    }
  }
}

