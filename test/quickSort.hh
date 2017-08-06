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


#ifndef __QUICKSORT_HH
#define __QUICKSORT_HH

#include <algorithm>
#include <functional>
#include "sequence-jobs.hh"
#include "Job.hh"
#include "utils.hh"

using namespace std;

template <class E, class BinPred>
void insertionSort(E* A, int n, BinPred f) {
  for (int i=0; i < n; i++) {
    E v = A[i];
    E* B = A + i;
    while (B-- > A && f(v,*B)) *(B+1) = *B;
    *(B+1) = v;
  }
}

#define LESS -1
#define EQUAL -2
#define MORE -3
template <class E>
class CompareWithPivot {
  const E pivot;  
public:
  CompareWithPivot (E pivot_) : pivot(pivot_) {}
  inline int operator() (const E x) const {
    if (x < pivot)
      return LESS;
    else if (pivot < x)
      return MORE;
    else
      return EQUAL;
  }
};

class PositionScanPlus {
  const int selector;
public:
  PositionScanPlus () : selector(0) {}  // DO NOT USE. JUST TO KEEP COMPILER HAPPY
  PositionScanPlus (int selector_) : selector(selector_) {}
  inline int operator() (const int& x, const int&y) {
    int x_plus=x; int y_plus=y;
    if (x<0) x_plus = x==selector ? 1 : 0;
    if (y<0) y_plus = y==selector ? 1 : 0;
    return x_plus + y_plus;
  }
};

template <class E>
class FilterLR : public HR2Job {

  E *In,*less,*equal,*more;
  int *less_pos, *more_pos;
  int *compared;  int n,finger;
  int stage; 

public:
  FilterLR (E *In_,
	  E* less_, E* equal_, E* more_,
	  int* less_pos_, int* more_pos_, int* compared_,
	  int finger_, int n_,  int stage_=0, 
	  bool del=true)
    : HR2Job (del),
      In(In_),less(less_),equal(equal_),more(more_),
      less_pos(less_pos_),more_pos(more_pos_), compared(compared_),
      finger(finger_),n(n_),stage(stage_)  {}

  lluint size (const int block_size) {
    return 28*(long) n;
  }
  lluint strand_size (const int block_size) {
    return size(block_size);
  }
  
  void function () {
    if (stage == 0) {
      if (n<_SCAN_BSIZE) {
	for (int i=0; i<n; ++i)
	  //	  if (less_pos[finger+i]<less_pos[finger+i+1])
	  // else if (more_pos[finger+i]<more_pos[finger+i+1])	     	  
	  if (compared[finger+i]==LESS)
	    *(less++)=*(In++);
	  else if (compared[finger+i]==MORE)
	    *(more++)=*(In++);
	  else
	    *(equal++)=*(In++);
	
	join ();
      } else {
	int mid = finger+n/2;
	int left_less = less_pos[mid]-less_pos[finger];
	int more_less = more_pos[mid]-more_pos[finger];
	binary_fork (new FilterLR<E>(In,less,equal,more,less_pos,more_pos,compared,finger,n/2,0),
		     new FilterLR<E>(In+n/2,less+left_less,equal+n/2-left_less-more_less,more+more_less,less_pos,more_pos,compared,mid,n-n/2,0),
		     new FilterLR<E>(In,less,equal,more,less_pos,more_pos,compared,finger,n,1));
      }
    } else {
      join ();
    }
  }
};

#define ISORT 32
#define QSORT_PAR_THRESHOLD (1<<14)

// If invert = 1, then A is sorted into B
template<class E, class BinPred>
void seqQuickSort(E* A, int n, BinPred f, E* B=NULL, bool invert=0) {
  if (n < ISORT) insertionSort(A, n, f);
  else {
    E p = A[n/2]; //std::__median(A[n/4],A[n/2],A[(3*n)/4],f);
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

template <class E>
lluint quickSortSize (int n, int block_size) {
  return 2*((lluint)(n+1))*sizeof(E) 
    + 3*((lluint)(n+1))*sizeof(int)
      + 5*block_size;
}

template <class E, class BinPred>
class QuickSort : public HR2Job {
  E *A, *B; int n;
  int *compared;
  int *less_pos, *more_pos;
  int *less_pos_n; 
  int *more_pos_n;
  BinPred f; bool invert;
  int stage;
  
public:
  QuickSort (E* A_, int n_, BinPred f_,
	     int* compared_=NULL, int* less_pos_=NULL, int* more_pos_=NULL, E* B_=NULL,
	     int stage_=0, bool invert_=0, bool del=true)
    : HR2Job (del), A(A_), n(n_), f(f_), 
      compared(compared_), less_pos(less_pos_), more_pos(more_pos_), B(B_), invert(invert_),
      stage(stage_) {}

  QuickSort (QuickSort *from, bool del=true)
    : HR2Job (del), A(from->A), B(from->B), n(from->n), f(from->f), 
      compared(from->compared), less_pos(from->less_pos), more_pos(from->more_pos), 
      less_pos_n(from->less_pos_n), more_pos_n(from->more_pos_n), invert(from->invert),
      stage(from->stage + 1) {}

  lluint size (const int block_size) {
    return round(quickSortSize<E>(n,block_size));
  }
  lluint strand_size (const int block_size) {
    return size(block_size);
  }
  
  inline E good_pivot (int sample_size) {
    E sample[sample_size];
    for (int i=0;i<sample_size;++i) sample[i]=A[i*(n/sample_size)];
    seqQuickSort(sample,sample_size,f);
    return sample[sample_size/2]; 
  }
  
  void function () {
    if (stage == 0) {
      if (n < QSORT_PAR_THRESHOLD) {
	seqQuickSort (A,n,f,B,invert);
	join ();
      } else {
	assert(compared!=NULL && less_pos!=NULL && more_pos!=NULL && B!=NULL);
      
	//E pivot = A[n/2]; 
	//E pivot = good_pivot(43);
	E pivot = good_pivot(3);

	unary_fork (new Map<E,int,CompareWithPivot<E> > (A,compared,n, CompareWithPivot<E>(pivot)  ),//comparator),
		    new QuickSort<E,BinPred>(this));
      }

    } else if (stage == 1) {

	PositionScanPlus lessScanPlus(LESS);

	
	less_pos_n = newA(int,1);
	more_pos_n = newA(int,1);
	unary_fork (new Scan<int,PositionScanPlus >(less_pos_n,compared,less_pos,n,lessScanPlus,0),
		    new QuickSort<E,BinPred>(this));

    } else if (stage == 2) {

	PositionScanPlus moreScanPlus(MORE);
	unary_fork (new Scan<int,PositionScanPlus >(more_pos_n,compared,more_pos,n,moreScanPlus,0),
		    new QuickSort<E,BinPred>(this));
	
    } else if (stage == 3) {

      unary_fork (new FilterLR<E> (A,B,B+*less_pos_n,B+n-*more_pos_n,less_pos,more_pos,compared,0,n),
		  new QuickSort<E,BinPred>(this));

    } else if (stage == 4) {
      int more_els=n-*more_pos_n;

      QuickSort<E,BinPred>* lessSort = new QuickSort<E,BinPred>(B,*less_pos_n,f,
								compared,less_pos,more_pos,
								A,0,!invert);
      QuickSort<E,BinPred>* moreSort = new QuickSort<E,BinPred>(B+more_els,*more_pos_n,f,
								compared+more_els,less_pos+more_els,more_pos+more_els,
								A+more_els,0,!invert);
      binary_fork (lessSort, moreSort, new QuickSort<E,BinPred>(this));

    } else if (stage == 5) {
      if (invert)
	join();
      else
	unary_fork (new Map<E,E,Id<E> >(B+*less_pos_n,A+*less_pos_n,n-*less_pos_n-*more_pos_n,Id<E>()),
		    new QuickSort<E,BinPred>(this));
      
    } else if (stage == 6) {

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

/* Source
   
// Quicksort based on median of three elements as pivot
//  and uses insertionSort for small inputs
template <class E, class BinPred>
void quickSort(E* A, int n, BinPred f) {
  if (n < ISORT) insertionSort(A, n, f);
  else {
    E p = std::__median(A[n/4],A[n/2],A[(3*n)/4],f);
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
    cilk_spawn quickSort(A, L-A, f);
    quickSort(M, A+n-M, f); // Exclude all elts that equal pivot
    cilk_sync;
  }
}
*/

#endif
