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


#include "ThreadPool.hh"
#include "quickSort2.hh"
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

template <class ET, class binPred>
class searchPivots {
  ET* A;
  ET* pivots;
  int numBuckets;
  binPred f;
public:
  searchPivots(ET* A_, ET* pivots_, int numBuckets_, binPred f_) :
    A(A_), pivots(pivots_), numBuckets(numBuckets_), f(f_) {}
  int operator () (int i) {
    return binSearch(pivots, numBuckets-1,A[i],f);
  }
};

template <class E, class BinPred>
class mapQuicksort : public HR2Job {
  E *A, *B;
  int *bucketStarts;
  int s; int n; 
  BinPred f;
  int stage;

public:
  mapQuicksort(E *A_, int *bucketStarts_, int s_, int n_, 
	       BinPred f_, E* B_=NULL, int stage_=0, 
	       bool del=true)
    : HR2Job (del), A(A_), bucketStarts(bucketStarts_), s(s_), n(n_), f(f_), 
      B(B_), stage(stage_)  {}

  lluint size (const int block_size) { 
    if (n==1) return 16*(bucketStarts[s+1] - bucketStarts[s]);
    else return 100000000;}
  lluint strand_size (const int block_size) { 
    return size(block_size);}
  
  void function () {
    if (stage == 0) {
      if (n==1) {
	//int td = example_get_time() -ts;
	//printf("i=%d: %d\n", s, td);	
	int size = bucketStarts[s+1] - bucketStarts[s];
	unary_fork(new QuickSort<E,BinPred>(A+bucketStarts[s],size,f,
					    B+bucketStarts[s]),
		   new mapQuicksort(A,bucketStarts,s,n,f,B,1));
      } else {
	binary_fork (new mapQuicksort(A,bucketStarts,s,n/2,f,B),
		     new mapQuicksort(A,bucketStarts,s+n/2,n-n/2,f,B),
		     new mapQuicksort(A,bucketStarts,s,n,f,B,1));
      }
    } else {
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
  
public:
  SampleSort (E* A_, E* B_, int* counts_, int* bucketId_,
	      int numBlocks_, int numBuckets_, int n_, 
	      BinPred f_, bool del=true)
    : HR2Job (del), A(A_), B(B_), counts(counts_), bucketId(bucketId_),
      numBlocks(numBlocks_), numBuckets(numBuckets_), n(n_), f(f_), 
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
      stage(from->stage+1) 
  {}

  lluint size (const int block_size) { 
    return 100000000; }

  lluint strand_size (const int block_size) { 
    return size(block_size); }

  SampleSort<E,BinPred>* nextStage() {stage++; return this;}
  
  void function () {
    if (stage == 0) {
      nextTime("Start");

      int overSample = 40;
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

      bucketStarts = newA(int, numBuckets+1);
      unary_fork(new Collect<E,int,searchPivots<E,BinPred> >
		 (A, B, n, numBuckets, bucketId, bucketStarts,
		  searchPivots<E,BinPred>(A, pivots, numBuckets, f),
		  counts, numBlocks),
		 new SampleSort(this));

    } else if (stage == 1) {
      nextTime("Collecting");
      ts = example_get_time();     
      unary_fork(new mapQuicksort<E,BinPred>(B,bucketStarts,0,numBuckets,f,A),
		 new SampleSort(this));

    } else if (stage == 2) {
      nextTime("Sorting Blocks");

      free(bucketStarts);
      join();

    }
  }
};

typedef double E;

int main (int argv, char **argc) {
  int LEN = (-1==get_size(argv, argc,2)) ? 100000000 : get_size(argv, argc,2);

  int numBlocks = 126;
  int bucketSize = 700000;
  int numBuckets = (LEN-1)/bucketSize + 1;
  E *A, *B;
  int *counts, *bucketId;

  #define blocks 5
  char* space[blocks];
  int alloc_sizes[blocks] = {sizeof(E)*(LEN+1),
			     sizeof(E)*(LEN+1),
			     sizeof(int)*(LEN+1),
			     sizeof(int)*(LEN+1), 10000000};

  for (int i=0;i<blocks;++i) {
    space[i] = (char*) newA (char, alloc_sizes[i]);
    stripe(space[i], alloc_sizes[i]);
  }
  
  A = (E*) space[0];
  B = (E*) space[1];
  bucketId = (int*) space[2];
  counts = (int*) space[3];
  free(space[4]);

  for (int i=0; i<LEN; ++i) {
    A[i] = utils::hash(i);   
    B[i] = 0;
    bucketId[i] = 0;
  }

  for (int i=0; i<numBuckets*numBlocks; ++i) {
    counts[i] = 0;
  }

  Scheduler *sched=create_scheduler (argv, argc);
  flush_cache(num_procs,sizes[1]);  
  cout<<"Len: "<<LEN<<endl;
  startTime();
  tp_init (num_procs, map, sched,
	   new SampleSort<E,less<E> >(A, B, counts, bucketId, numBlocks, 
				      numBuckets, LEN, less<E>()));
  tp_sync_all ();
  nextTime("Total time, measured from driver program");

  std::cout<<"Checking: "<<std::endl;
  if (checkSort (B,LEN,less_equal<E>()))
    std::cout<<"Good"<<std::endl;
  else
    std::cout<<"Bad"<<std::endl;
}
