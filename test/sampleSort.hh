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


#include <iostream>
#include <algorithm>
#include "utils.hh"
#include "sequence-jobs.hh"
#include "gettime.hh"
#include "math.h"
#include "quickSort.hh"
#include "transpose.hh"
#include "merge.hh"


template<class E>
lluint sampleSortSizeInBytes (int n) {
//  return .6*sizeof(E)*(lluint)(n);
  return (lluint)(n) * (3*sizeof(E)+3*sizeof(int));
}

template <class E, class BinPred> class SampleSort;

template<class E, class BinPred>
void mergeSeq (E* sA, int lA, E* sB, int lB, int* sC, BinPred f) {
  if (lA==0 || lB==0) return;
  E *eA = sA+lA;
  E *eB = sB+lB;
  for (int i=0; i <= lB; i++) sC[i] = 0;
  while(1) {
    while (f(*sA, *sB)) {(*sC)++; if (++sA == eA) return;}
    sB++; sC++;
    if (sB == eB) break;
    if (!(f(*(sB-1),*sB))) {
      while (!f(*sB, *sA)) {(*sC)++; if (++sA == eA) return;}
      sB++; sC++;
      if (sB == eB) break;
    }
  } 
  *sC = eA-sA;
}

template <class E, class BinPred>
class SortAndMerge : public HR2Job {

  E* A; int offset; int A_size; BinPred f;
  E* pivots; int* segSizes; int numSegs;
  int stage;
  
public:

  SortAndMerge ( E* A_, int size_, BinPred f_,
		 E* pivots_, int*segSizes_, int numSegs_,
		 int stage_=0, bool del=true)
    : HR2Job (del),
      A(A_), A_size(size_), f(f_),
      pivots(pivots_), segSizes(segSizes_), numSegs(numSegs_), stage(stage_)
    {}

  SortAndMerge (SortAndMerge* from, bool del=true)
    : HR2Job (del),
      A(from->A), offset(from->offset), A_size(from->A_size), f(from->f),
      pivots (from->pivots), segSizes (from->segSizes), numSegs (from->numSegs),
      stage (from->stage+1)
    {}
  
  lluint size (const int block_size) {
    return round_up(sampleSortSizeInBytes<E>(A_size), block_size)
      + round_up(numSegs*sizeof(E), block_size)
      + round_up(numSegs*sizeof(int), block_size);
  }
  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size(block_size);
    } else {
      if (stage==0)
	return STRAND_SIZE;
      else
	return size(block_size);
    }
  }
  
  void function () {
    if (stage==0) {
      unary_fork (new SampleSort<E,BinPred> (A,A_size,f),
		  new SortAndMerge(this));
    } else {
      mergeSeq<E, BinPred> (A,A_size,pivots,numSegs-1,segSizes,f);
      join();
    } 
  }
};

template <class E, class BinPred>
class GenerateSortAndMergeObjects : public JobGenerator {

  E* A; BinPred f;
  E* pivots; int*segSizes; int numSegs, rowSize, numR, n;

public :

  GenerateSortAndMergeObjects (E* A_, BinPred f_,
			       E* pivots_, int* segSizes_,
			       int numSegs_, int rowSize_, int numR_, int n_)			     
    : A(A_), f(f_), pivots(pivots_), segSizes(segSizes_),
      numSegs(numSegs_), rowSize(rowSize_), numR(numR_), n(n_)
    {}

  HR2Job* operator()(int r) {
    //int offset = r * rowSize;
    //int size =  (r < numR - 1) ? rowSize : n - offset;
    return new SortAndMerge<E, BinPred> (A+r*rowSize, (r < numR - 1) ? rowSize : n - r*rowSize,
					 f, pivots, segSizes + r*numSegs, numSegs);
  }
};

template <class E, class BinPred>
class GenerateSortObjects2 : public JobGenerator {

  E* A;
  int *offsetB; int numR; BinPred f; int n; int numSegs;
  E* pivots;
  
public :

  GenerateSortObjects2 (E* A_, E* pivots_, int *offsetB_,
			int numSegs_, int numR_, int n_, BinPred f_)
    : A(A_), offsetB(offsetB_),
      numSegs(numSegs_), numR(numR_), pivots(pivots_), n(n_), f(f_)
    {}
  
  inline HR2Job*  operator()(int i) {
    if (i == 0) {
      return new SampleSort<E, BinPred> (A, offsetB[numR], f); // first segment
    } else if (i < numSegs-1) { // middle segments
      // if not all equal in the segment
      if (f(pivots[i-1],pivots[i])) 
	return new SampleSort<E, BinPred> (A+offsetB[i*numR], offsetB[(i+1)*numR] - offsetB[i*numR], f);
    } else { // last segment
      //std::cout<<"Last: "<<n<<" "<< n - offsetB[i*numR]<<std::endl;
      return new SampleSort<E, BinPred> (A+offsetB[i*numR], n - offsetB[i*numR], f);
    }
  }
};

#define SSORT_THR 200000
#define AVG_SEG_SIZE 2
#define PIVOT_QUOT 2
#define OVERSAMPLE 4

template<class E, class BinPred>
class SampleSort : public HR2Job {

  E* A; int n; BinPred f; double exp;
  int sq, rowSize, numR, numSegs, overSample, sampleSetSize;
  E *sampleSet;  E *pivots;  E *B;
  int *segSizes;  int *offsetA;  int *offsetB;
  HR2Job ***first_sort_objects;  lluint **first_sort_size_sums_ptr;
  HR2Job ***second_sort_objects;  lluint **second_sort_size_sums_ptr;
  HR2Job **merge_objects;
  lluint *bt_sizes; char* space;
  int stage;

public:
  enum SORT_STAGES {
    ST_SORT_SAMPLES = 10,
    ST_SUBSELECT ,
    ST_FIRST_SORT,
    ST_A_OFFSETS,
    ST_A_TRANSPOSE,
    ST_B_OFFSETS,
    ST_BLOCK_TRANSPOSE_SIZE,
    ST_BLOCK_TRANSPOSE,
    ST_COPY_BACK,
    ST_SECOND_SORT_SIZING,
    ST_SECOND_SORT,
    ST_CLEAN_UP,
    ST_END
  };
  
  SampleSort (E* A_, int n_, BinPred f_, double exp_=0.5, char* space_= NULL,
	      bool del=true)
    : HR2Job (del),
      A (A_), n(n_), f(f_), exp(exp_),
      sampleSet(NULL), pivots(NULL), B(NULL),
      segSizes(NULL), offsetA(NULL), offsetB(NULL),
      space(space_), stage (0)
    {}
  
  SampleSort (SampleSort* from,int stage_=-1,
	      bool del=true)
    : HR2Job (del),
      A(from->A), n(from->n), f(from->f), exp(from->exp),
      sq(from->sq), rowSize(from->rowSize), numR(from->numR), numSegs(from->numSegs), overSample(from->overSample),
      sampleSetSize(from->sampleSetSize), sampleSet(from->sampleSet), 
      pivots(from->pivots),  B(from->B),
      segSizes(from->segSizes), offsetA(from->offsetA), offsetB(from->offsetB),
      first_sort_objects (from->first_sort_objects), first_sort_size_sums_ptr (from->first_sort_size_sums_ptr),
      second_sort_objects (from->second_sort_objects), second_sort_size_sums_ptr (from->second_sort_size_sums_ptr),
      merge_objects(from->merge_objects), space(from->space),
      bt_sizes(from->bt_sizes) {
    if (stage_==-1)
      stage = from->stage+1;
    else
      stage = stage_;
  }      
  
  lluint size (const int block_size) {
    return round_up (sampleSortSizeInBytes<E>(n), block_size);
  }

  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size (block_size);
    } else {
      return STRAND_SIZE;
    } 
  }

  void function () {
    if (stage == 0) {

      if (n < SSORT_THR) {
	//unary_fork (new QuickSort<E, BinPred>(A, n, f), new SampleSort<E, BinPred>(this,ST_END));
	seqQuickSort (A,n,f);
	join();
      } else {
	sq = (int)(pow(n,exp));
	rowSize = sq*AVG_SEG_SIZE;
	numR = (int)ceil(((double)n)/((double)rowSize));
	numSegs = (sq-1)/PIVOT_QUOT;
	overSample = OVERSAMPLE;
	sampleSetSize = numSegs*overSample;

	if (space == NULL) {
	  sampleSet = newA(E,sampleSetSize);
	} else {
	  sampleSet = (E*) space;
	  space += sizeof(E)*sampleSetSize;
	}
	
	Hash<E> hash(A, n);
	unary_fork (new MapInt<E,Hash<E> > (sampleSet, sampleSetSize, hash),
		    new SampleSort<E, BinPred> (this,ST_SORT_SAMPLES));
      }

    } else if (stage == ST_SORT_SAMPLES) {

      unary_fork (new QuickSort<E, BinPred> (sampleSet, sampleSetSize, f),
		  new SampleSort<E, BinPred> (this));
       
    } else if (stage == ST_SUBSELECT) {
      if (space == NULL) {
	pivots = newA(E,numSegs-1);
      } else {
	pivots = (E*)space;
	space += sizeof(E)*(numSegs-1);
      }

      for (int k=0; k < numSegs-1; ++k) {
	int o = overSample*k;
	pivots[k] = sampleSet[o];
      }
      
      if (space == NULL) {
	free(sampleSet);  
	B = newA(E, numR*rowSize);
	segSizes = newA(int, numR*numSegs+1);
	offsetA = newA(int, numR*numSegs+1);
	offsetB = newA(int, numR*numSegs+1);
      } else {
	B = (E*)space; space += sizeof(E)*(numR*rowSize);
	segSizes = (int*)space; space += sizeof(int)*(numR*numSegs+1);
	offsetA = (int*)space; space += sizeof(int)*(numR*numSegs+1);
	offsetB = (int*)space; space += sizeof(int)*(numR*numSegs+1);
      }
	
      first_sort_objects = new HR2Job**;
      first_sort_size_sums_ptr = new lluint*;
      unary_fork ( new Pfor (new GenerateSortAndMergeObjects<E, BinPred>
			     (A, f, pivots, segSizes, numSegs, rowSize, numR, n),
			     numR, NULL, ST_GEN_JOBS, first_sort_objects, first_sort_size_sums_ptr),
		   new SampleSort<E,BinPred> (this));

    } else if (stage == ST_FIRST_SORT) {

      unary_fork (new Pfor (*first_sort_objects, numR, *first_sort_size_sums_ptr),
		  new SampleSort<E,BinPred> (this));
      
    } else if (stage == ST_A_OFFSETS) {

      unary_fork (new Scan<int, plus<int> > (new int, segSizes, offsetA, numR*numSegs+1, plus<int>(),0),
		  new SampleSort<E, BinPred> (this));

    } else if (stage == ST_A_TRANSPOSE) {

      unary_fork (new Transpose<int>(segSizes, offsetB, numR, numSegs),
		  new SampleSort<E, BinPred> (this));

    } else if (stage == ST_B_OFFSETS) {

      unary_fork (new Scan<int, plus<int> > (new int, offsetB, offsetB, numR*numSegs+1, plus<int>(),0),
		  new SampleSort<E, BinPred> (this));

    } else if (stage == ST_BLOCK_TRANSPOSE_SIZE) {

      bt_sizes = new lluint [8*numR*numSegs/_PAR_TRANS_THRESHHOLD];
      unary_fork (new BlockTranspose<E>(A, B, offsetA, offsetB, segSizes, numR, numSegs, BT_ST_SIZE, bt_sizes),
		  new SampleSort<E, BinPred> (this));

    } else if (stage == ST_BLOCK_TRANSPOSE) {

      unary_fork (new BlockTranspose<E>(A, B, offsetA, offsetB, segSizes, numR, numSegs, 0, bt_sizes),
		  new SampleSort<E, BinPred> (this));

    } else if (stage == ST_COPY_BACK) {

      unary_fork (new Map<E, E, Id<E> > (B, A, n, Id<E>() ),
		  new SampleSort<E, BinPred> (this));

    } else if (stage == ST_SECOND_SORT_SIZING) {

      second_sort_objects = new HR2Job**;
      second_sort_size_sums_ptr = new lluint*;
      unary_fork (new Pfor (new GenerateSortObjects2<E,BinPred>
			    (A, pivots, offsetB, numSegs, numR, n, f),
			    numSegs, NULL, ST_GEN_JOBS, second_sort_objects, second_sort_size_sums_ptr),
		  new SampleSort<E, BinPred> (this));

    } else if (stage == ST_SECOND_SORT) {

      unary_fork (new Pfor (*second_sort_objects, numSegs, *second_sort_size_sums_ptr),
		  new SampleSort<E, BinPred> (this));
      
    } else if (stage == ST_CLEAN_UP) {
      if (space == NULL)  {
	free(pivots); free(offsetB); 
	free(B); free(offsetA); free(segSizes);
      }
      join();

    } else if (stage == ST_END) {

      join();

    } else {

      std::cout<<"Invalid Stage"<<std::endl;
      exit(-1);
    }
  }
};
    
#undef compSort
#define compSort(__A, __n, __f) (sampleSort(__A, __n, __f))
