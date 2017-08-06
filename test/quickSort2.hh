#ifndef __QUICKSORT_HH
#define __QUICKSORT_HH

#include <algorithm>
#include <functional>
#include "sequence-jobs.hh"
#include "collect.hh"
#include "Job.hh"
#include "utils.hh"

using namespace std;

long min(long a, long b) {
  if (a<b) return a;
  else return b;
}

template <class E, class BinPred>
class genCounts {
  E *A;
  int *Counts;
  E pivot;
  int n, numBlocks;
  BinPred f;

public:
  genCounts(E *A_, int* Counts_, E pivot_, 
	    int n_, int numBlocks_, BinPred f_) :
    A(A_), Counts(Counts_), pivot(pivot_), 
    n(n_), numBlocks(numBlocks_), f(f_)
  {}

  void operator() (int i) {
    int blockSize = (n-1)/numBlocks + 1;
    int offset = i * blockSize;
    int size =  (i < numBlocks - 1) ? blockSize : n - offset; 
    int lessCnt = 0;
    int greaterCnt = 0;
    for (int j = offset; j < offset + size; j++) {
      if (f(A[j],pivot)) lessCnt++;
      else if (f(pivot,A[j])) greaterCnt++;
    }
    Counts[3*i] = lessCnt;
    Counts[3*i+1] = size - lessCnt - greaterCnt;
    Counts[3*i+2] = greaterCnt;
    return;
  }
};

template <class E, class BinPred>
class relocate {
  E *A, *B;
  int *Counts;
  E pivot;
  int n, numBlocks;
  BinPred f;

public:
  relocate(E *A_, E *B_, int* Counts_, E pivot_,
	    int n_, int numBlocks_, BinPred f_) :
    A(A_), B(B_), Counts(Counts_), pivot(pivot_),
    n(n_), numBlocks(numBlocks_), f(f_)
  {}

  void operator() (int i) {
    int blockSize = (n-1)/numBlocks + 1;
    int offset = i * blockSize;
    int size =  (i < numBlocks - 1) ? blockSize : n - offset; 
    int lessCnt = Counts[3*i];
    int equalCnt = Counts[3*i+1];
    int grtCnt = Counts[3*i+2];
    for (int j = offset; j < offset + size; j++) {
      if (f(A[j],pivot)) B[lessCnt++] = A[j];
      else if (f(pivot,A[j])) B[grtCnt++] = A[j];
      else B[equalCnt++] = A[j];
    }
    return;
  }
};

#define ISORT 32
#define QSORT_SEQ_THRESHOLD (1<<11)
#define QSORT_SPLIT_THRESHOLD (1<<17)

template <class E, class BinPred>
void insertionSort(E* A, int n, BinPred f) {
  for (int i=0; i < n; i++) {
    E v = A[i];
    E* B = A + i;
    while (B-- > A && f(v,*B)) *(B+1) = *B;
    *(B+1) = v;
  }
}

template <class E, class BinPred>
E median(E a, E b, E c, BinPred f) {
  return  f(a,b) ? (f(b,c) ? b : (f(a,c) ? c : a)) 
    : (f(a,c) ? a : (f(b,c) ? c : b));
}

// If invert = 1, then A is sorted into B
template<class E, class BinPred>
void seqQuickSort(E* A, int n, BinPred f, E* B=NULL, bool invert=0) {
  if (n < ISORT) insertionSort(A, n, f);
  else {
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
    seqQuickSort(A, L-A, f);
    seqQuickSort(M, A+n-M, f); // Exclude all elts that equal pivot
  }
  if (invert==1) {
    for (int i=0;i<n;++i)
      B[i]=A[i];
  }
}

#define GOOD_PIVOT 0//1

template <class E, class BinPred>
inline E getPivot (E* A, int n, BinPred f) {
  if (GOOD_PIVOT) {
    int sample_size = 43;
    E sample[sample_size];
    for (int i=0;i<sample_size;++i) sample[i]=A[i*(n/sample_size)];
    seqQuickSort(sample,sample_size,f);
    return sample[sample_size/2]; 
  } else 
    return median(A[n/4],A[n/2],A[(3*n)/4],f);
}

template <class E, class BinPred>
pair<E*,E*> split(E* A, int n, BinPred f) {
  E p = getPivot(A, n, f);
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

template <class E, class BinPred>
class QuickSort : public HR2Job {
  E *A, *B; int n;
  int *counts;
  E pivot;
  int numBlocks;
  BinPred f; bool invert;
  int stage;
  
public:
  QuickSort (E* A_, int n_, BinPred f_, E* B_,
	     int stage_=0, bool invert_=0, bool del=true)
    : HR2Job (del), A(A_), n(n_), f(f_), 
      B(B_), invert(invert_), stage(stage_) {}

  QuickSort (QuickSort *from, bool del=true)
    : HR2Job (del), A(from->A), B(from->B), n(from->n), f(from->f), 
      pivot(from->pivot), counts(from->counts), invert(from->invert), 
      numBlocks(from->numBlocks), stage(from->stage + 1) {}

  lluint size (const int block_size) {
      if (n < QSORT_SPLIT_THRESHOLD) 
	return 8 * n;
      else return 16 * n;
  }

  lluint strand_size (const int block_size) { 
    return size(block_size); }
  
  void function () {
    if (stage == 0) {
      if (n < QSORT_SEQ_THRESHOLD) {
	seqQuickSort (A,n,f,B,invert);
	join ();
      } else if (n < QSORT_SPLIT_THRESHOLD) {
	// copy elements if needed
	if (invert) {
	  for (int i=0; i < n; i++) B[i] = A[i];
	  A = B;
	}
	pair<E*,E*> X = split(A,n,f);
	binary_fork (new QuickSort(A, X.first -A, f, B),
		     new QuickSort(X.second, A+n-X.second,f, B),
		     new QuickSort(A,n,f,B,3));
      } else {
	pivot = getPivot(A,n,f);
	numBlocks = min(126, 1 + n/20000);
	counts = newA(int,3*numBlocks);
	ts = example_get_time();     
	mapIdx(numBlocks, 
	       genCounts<E,BinPred>(A, counts, pivot, n, numBlocks, f),
	       new QuickSort(this), this);
      }
	
    } else if (stage == 1) {
      int sum = 0;
      for (int i = 0; i < 3; i++)
	for (int j = 0; j < numBlocks; j++) {
	  int v = counts[j*3+i];
	  counts[j*3+i] = sum;
	  sum += v;
	}

      ts = example_get_time();     
      mapIdx(numBlocks, 
	     relocate<E,BinPred>(A, B, counts, pivot, n, numBlocks,f),
	     new QuickSort(this), this);

    } else if (stage == 2) {
      int nLess = counts[1];
      int oGreater = counts[2];
      int nGreater = n - oGreater;
      if (counts[3*numBlocks-1] != n)
	std::cout<<counts[3*numBlocks-1]<<" "<<n<<std::endl;
      assert(counts[3*numBlocks-1] == n);
      free(counts);
      // copy equal elements if needed
      if (!invert) 
	for (int i=nLess; i < oGreater; i++)
	  A[i] = B[i];
	
      binary_fork (new QuickSort(B, nLess, f, A, 0, !invert),
		   new QuickSort(B+oGreater,nGreater,f,A+oGreater,0, !invert),
		   new QuickSort(this));

    } else if (stage == 3) {
      join();
    }
  }
};

template <class E, class BinPred>
bool
checkSort (E* A, int n, BinPred f) {
  for (int i=0; i<n-1; ++i) 
    if (!f(A[i], A[i+1])) {
      std::cout<<"Sort check failed at: "<<i<<std::endl;
      return false;
    }
  return true;
}

#endif
