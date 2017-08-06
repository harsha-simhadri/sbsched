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


#ifndef A_RADIX_INCLUDED
#define A_RADIX_INCLUDED

#include <iostream>
#include <math.h>
#include "gettime.h"
#include "sequence-jobs.hh"
#include "utils.hh"
#include "transpose.hh"
//#include "sequential.hh"
using namespace std;



namespace intSort {

  // Cannot be greater than 8 without changing definition of bIndexT
  //    from unsigned char to unsigned int (or unsigned short)
#define MAX_RADIX 8 
#define BUCKETS 256    // 1 << MAX_RADIX
#define SMALLFOR_SEQ_THRESH 100
#define RADIXSTEP_SEQ_THRESH 1

  typedef int bucketsT[BUCKETS];

  // a type that must hold MAX_RADIX bits
  typedef unsigned char bIndexT;

  class NullJob : public SizedJob {
    lluint s;
  public:
    NullJob(lluint s_) : SizedJob(true),s(s_) {}
    
    void function() {
      join();
    }
    
    lluint size (const int block_size) {
      return s;
    }
  };

  template <class E, class F>
  void radixBlock(E* A, E* B, bIndexT *Tmp, bucketsT counts, bucketsT offsets,
		  int Boffset, int n, int m, F extract) {
    for (int i = 0; i < m; i++)  counts[i] = 0;
    for (int j = 0; j < n; j++) {
      int k = Tmp[j] = extract(A[j]);
      counts[k]++;
    }
    int s = Boffset;
    for (int i = 0; i < m; i++) {
      s += counts[i];
      offsets[i] = s;
    }
    for (int j = n-1; j >= 0; j--) {
      int x =  --offsets[Tmp[j]];
      B[x] = A[j];
    }
  }

  template <class E, class F>
  void radixStepSerial(E* A, E* B, bIndexT *Tmp, bucketsT buckets,
		       int n, int m, F extract) {
    radixBlock(A, B, Tmp, buckets, buckets, 0, n, m, extract);
    for (int i=0; i < n; i++) A[i] = B[i];
    return;
  }

  template <class E, class F>
  class RadixStepR : public SizedJob {
    E* A; E* B; bIndexT *Tmp; int* cnts; int *oB; int nn; int imin; int imax; int m; int nni; F extract; 
  public:
    RadixStepR(E* A_, E* B_, bIndexT *Tmp_, int* cnts_, int* oB_, int nn_, int m_, F extract_, int imax_, int nni_, bool del=true) 
      : SizedJob(del),
	A(A_), B(B_), Tmp(Tmp_), cnts(cnts_), oB(oB_), nn(nn_), imin(0), imax(imax_), m(m_), nni(nni_), extract(extract_)
    {}
    
    RadixStepR(RadixStepR *from, int imin_, int imax_, int nni_, bool del=true) 
      : A(from->A), B(from->B), Tmp(from->Tmp), cnts(from->cnts), oB(from->oB), nn(from->nn), m(from->m), extract(from->extract), imin(imin_), imax(imax_), nni(nni_) 
    {}

    lluint size (const int block_size) {
      //      return 1;
      // Only touches nni stuff in each of A and B and Tmp
      // Each leaf touches m stuff in cnts and oB
      // There are imax-imin leaves

      return nni * (2*sizeof(E) + sizeof(bIndexT)) + 2*m*(imax-imin)*sizeof(int);
    }
    
    void function() {
      if (imax - imin <= RADIXSTEP_SEQ_THRESH) {
	for (int i = imin; i < imax; i++) {
	  int od = i*nn;
	  radixBlock(A+od, B, Tmp+od, cnts + m*i, oB + m*i, od, nni, m, extract);
	}
	join();
      } else {
	int imid = (imin + imax) / 2;
	int nnimid = nn*(imid-imin);
	binary_fork (new RadixStepR(this, imin, imid, nnimid),
		     new RadixStepR(this, imid, imax, nni - nnimid),
		     new NullJob(size(1)));
      }
    }
  };

  // A is the input and sorted output (length = n)
  // B is temporary space for copying data (length = n)
  // Tmp is temporary space for extracting the bytes (length = n)
  // BK is an array of bucket sets, each set has BUCKETS integers
  //    it is used for temporary space for bucket counts and offsets
  // numBK is the length of BK (number of sets of buckets)
  // the first entry of BK is also used to return the offset of each bucket
  // m is the number of buckets per set (m <= BUCKETS)
  // extract is a function that extract the appropriate bits from A
  //  it must return a non-negative integer less than m
  template <class E, class F>
  class RadixStep : public SizedJob {
    E* A; E* B; bIndexT *Tmp; bucketsT *BK; int numBK; int n; int m; F extract;
    lluint **bt_sizes_ptr;
    int stage;
    int* ss;

  public:
    RadixStep(E* A_, E* B_, bIndexT *Tmp_, bucketsT *BK_,
	      int numBK_, int n_, int m_, F extract_, bool del=true) 
      : stage(0), SizedJob(del),
	A(A_), B(B_), Tmp(Tmp_), BK(BK_), numBK(numBK_), n(n_), m(m_), extract(extract_), bt_sizes_ptr(NULL)
    {}

    RadixStep(RadixStep *from, bool del=true) 
      : stage(from->stage+1), SizedJob(del), ss(from->ss),
	A(from->A), B(from->B), Tmp(from->Tmp), BK(from->BK), numBK(from->numBK), n(from->n), m(from->m), extract(from->extract),
	bt_sizes_ptr(from->bt_sizes_ptr)
    {}

    lluint size (const int block_size) {
      //      return 1;
      // A,B, and Tmp are size n
      // We divide BK into blocks. For each block, we touch about m stuff in 
      // each of 3 different arrays.
      int expand = (sizeof(E)<=4) ? 64 : 32;
      int blocks = min(numBK/3,(1+n/(BUCKETS*expand)));

      return n*(2*sizeof(E) + sizeof(bIndexT)) + 3*m*blocks*sizeof(int);
    }

    void function() {
      int expand = (sizeof(E)<=4) ? 64 : 32;
      int blocks = min(numBK/3,(1+n/(BUCKETS*expand)));
      int nn = (n+blocks-1)/blocks;
      int* cnts = (int*) BK;
      int* oA = (int*) (BK+blocks);
      int* oB = (int*) (BK+2*blocks);

      // std::cout<<"RadixStep Stage: "<<stage<<std::endl;
      
      if (stage == 0) {
	// need 3 bucket sets per block
	
	if (blocks < 2) {
	  radixStepSerial(A, B, Tmp, BK[0], n, m, extract);
	  join();
	  return;
	}

	/* IF I NEED TO RUN SEQUENTIALLY AS A TEST
	for (int i=0; i<blocks; i++) {
	  int od = i*nn;
	  int nni = min(max(n-od,0),nn);
	  radixBlock(A+od,B,Tmp+od,cnts+m*i,oB+m*i,od,nni,m,extract);
	}
	*/

	unary_fork(new RadixStepR<E,F> (A,B,Tmp,cnts,oB,nn,m,extract,blocks,n),
		   new RadixStep<E,F>(this));
      } else if (stage == 1) {
	/* IF I NEED TO RUN SEQUENTIALLY AS A TEST
	sequentialTranspose<int>(cnts,oA).trans(blocks,m);
	*/
	unary_fork(new Transpose<int>(cnts,oA,blocks,m),
		   new RadixStep<E,F>(this));
      } else if (stage == 2) {
	/* IF I NEED TO RUN SEQUENTIALLY AS A TEST
	sequentialScan<int,utils::addF<int> >(oA,oA,blocks*m, utils::addF<int>(),0);
	*/

	//ss is the result of the scan. not used anywhere, but I guess
	//we need to pass it around anyway
	ss = new int; 
	unary_fork(new Scan<int,utils::addF<int> >(ss,oA,oA,blocks*m, utils::addF<int>(),0),
		   new RadixStep<E,F>(this));
      } else if (stage == 3) {
	/*IF I NEED TO RUN SEQUENTIALLY AS A TEST 
/*	sequentialBlockTrans<E>(B,A,oB,oA,cnts).trans(blocks,m);
	for (int j=0; j<m; j++) BK[0][j] = oA[j*blocks];
	join();
	return;
	*/
	delete ss;
	bt_sizes_ptr = new lluint*;
	unary_fork(new BlockTranspose<E>(B,A,oB,oA,cnts,blocks,m,bt_sizes_ptr),
		   new RadixStep<E,F>(this));

      } else if (stage == 4) {

	unary_fork(new BlockTranspose<E>(B,A,oB,oA,cnts,blocks,m,0,*bt_sizes_ptr),
		   new RadixStep<E,F>(this));
	
      } else if (stage == 5) {
	// put the offsets for each bucket in the first bucket set of BK
	for (int j = 0; j < m; j++) BK[0][j] = oA[j*blocks];
	join();
      }
    }
  };
  
  // a function to extract "bits" bits starting at bit location "offset"
  template <class E, class F>
  struct eBits {
    F _f;  int _mask;  int _offset;
    eBits(int bits, int offset, F f): _mask((1<<bits)-1), 
				      _offset(offset), _f(f) {}
    int operator() (E p) {return _mask&(_f(p)>>_offset);}
  };
  
  
  // Radix sort with low order bits first
  template <class E, class F>
  class RadixLoopBottomUp : public SizedJob {
    E *A; E *B; bIndexT *Tmp; bucketsT *BK; int numBK; int n; int bits; F f;
    int bitOffset;

  public:
    //stage 0 constructor
    RadixLoopBottomUp(E *A_, E *B_, bIndexT *Tmp_, bucketsT *BK_,
		      int numBK_, int n_, int bits_, F f_, bool del=true) 
      : SizedJob(del),
	A(A_), B(B_), Tmp(Tmp_), BK(BK_), numBK(numBK_), n(n_), bits(bits_), f(f_), bitOffset(0) 
    {}
    
    // stage i constructor
    RadixLoopBottomUp(RadixLoopBottomUp* from, int bitOffset_, bool del=true) 
      : SizedJob(del), bitOffset(bitOffset_),
	A(from->A), B(from->B), Tmp(from->Tmp), BK(from->BK), numBK(from->numBK), n(from->n), bits(from->bits), f(from->f)
    {}
    
    lluint size (const int block_size) {
      // size here is dominated by the recursive call to RadixStep
      // the radix step has space in terms of these blocks:
      int expand = (sizeof(E)<=4) ? 64 : 32;
      int blocks = min(numBK/3,(1+n/(BUCKETS*expand)));

      // the m for the radix step depends on the number of bits here
      int rounds = 1+(bits-1)/MAX_RADIX;
      int rbits = 1+(bits-1)/rounds;
      int m = 1<<rbits;
      
      //RadixStep's space given these same parameters

      return n*(2*sizeof(E) + sizeof(bIndexT)) + 3*m*blocks*sizeof(int);

      //Note: the second term is the only part that depends on the blocks
      //and m, and this is just corresponding to space in the BK array.
      //instead it may be better to round up
      return n*(2*sizeof(E) + sizeof(bIndexT)) + numBK*BUCKETS*sizeof(int);
    }

    void function() {
      int rounds = 1+(bits-1)/MAX_RADIX;
      int rbits = 1+(bits-1)/rounds;
      if (bitOffset < bits) {
	if (bitOffset+rbits > bits) rbits = bits-bitOffset;
	unary_fork(new RadixStep<E,eBits<E,F> >(A,B,Tmp,BK,numBK, n, 1<<rbits, eBits<E,F>(rbits,bitOffset,f)),
		   new RadixLoopBottomUp<E,F>(this,bitOffset+rbits));
      } else {
	join();
      }
    }
  };

  template <class E, class F>
  class RadixLoopTopDownR :public SizedJob {
    E *A; E*B; bIndexT *Tmp; bucketsT *BK; int numBK; int n; int bits; F f;
    int imin; int imax;
  public:
    RadixLoopTopDownR(E *A_, E *B_, bIndexT *Tmp_, bucketsT *BK_,
		      int numBK_, int n_, int bits_, F f_, int imax_, bool del=true)
      : SizedJob(del),
	A(A_), B(B_), Tmp(Tmp_), BK(BK_), numBK(numBK_), n(n_), bits(bits_), f(f_), imin(0), imax(imax_)
    {}

    RadixLoopTopDownR(RadixLoopTopDownR* from, int imin_, int imax_, bool del=true) 
      : SizedJob(del),
	A(from->A), B(from->B), Tmp(from->Tmp), BK(from->BK), numBK(from->numBK), n(from->n), bits(from->bits), f(from->f), imin(imin_), imax(imax_)
    {}

    lluint size (const int block_size) { 
      // This ends up calling RadixLoopTopDown at each leaf of recursion
      // the total n there is:
      int* offsets = BK[0];
      int segOffset = offsets[imin];
      int segNextOffset = (imax == BUCKETS) ? n : offsets[imax];
      int segLen = segNextOffset - segOffset;
      
      // the total numBK across all leaves is 
      int remain = numBK - BUCKETS - 1;
      float y = remain / (float) n;
      int blocksOffset = ((int) floor(segOffset * y)) + imin + 1;
      int blocksNextOffset = ((int) floor(segNextOffset * y)) + imax + 1;
      int blockLen = blocksNextOffset - blocksOffset;
      
      // Space is dominated by the radix step, so as before
      int expand = (sizeof(E)<=4) ? 64 : 32;
      int blocks = min(blockLen/3,(1+n/(BUCKETS*expand)));
      // (Can round this up to blockLen/3) 

      int m = (bits < MAX_RADIX) ? (1<<bits) : (1<<MAX_RADIX);

      return n*(2*sizeof(E) + sizeof(bIndexT)) + 3*m*blocks*sizeof(int);
    }

    void function();
   };
    
  template <class E, class F>
  class RadixLoopTopDown : public SizedJob {
    
    E *A; E*B; bIndexT *Tmp; bucketsT *BK; int numBK; int n; int bits; F f;
  public :
    RadixLoopTopDown(E *A_, E *B_, bIndexT *Tmp_, bucketsT *BK_,
		     int numBK_, int n_, int bits_, F f_, bool del=true)
      : SizedJob(del),
	A(A_), B(B_), Tmp(Tmp_), BK(BK_), numBK(numBK_), n(n_), bits(bits_), f(f_)
    {}
    
    lluint size (const int block_size) {
      // Space is dominated by the radix step, so as before
      int expand = (sizeof(E)<=4) ? 64 : 32;
      int blocks = min(numBK/3,(1+n/(BUCKETS*expand)));
      // (Can round this up to numBK/3) 
      
      // m for the radixStep depends on the number of bits, but it's at most
      // the max radix
      int m = (bits < MAX_RADIX) ? (1<<bits) : (1<<MAX_RADIX);


      return n*(2*sizeof(E) + sizeof(bIndexT)) + 3*m*blocks*sizeof(int);
    }

    void function() {
      if (n == 0) {
	join();
	return;
      }
      if (bits <= MAX_RADIX) {
	unary_fork(new RadixStep<E,eBits<E,F> >(A,B,Tmp,BK,numBK,n,1<<bits,eBits<E,F>(bits,0,f)),
		   new NullJob(size(1)));
      } else if (numBK >= BUCKETS+1) {
	unary_fork(new RadixStep<E,eBits<E,F> >(A,B,Tmp,BK,numBK,n,BUCKETS,eBits<E,F>(MAX_RADIX,bits-MAX_RADIX,f)),
		   new RadixLoopTopDownR<E,F>(A,B,Tmp,BK,numBK,n,bits,f, BUCKETS));
      } else { 
	unary_fork(new RadixLoopBottomUp<E,F>(A,B,Tmp,BK,numBK,n,bits,f),
		   new NullJob(size(1)));
      }
    }
  };

  template <class E, class F>
  void RadixLoopTopDownR<E,F>::function() {
      if (imin == imax-1) {
	int i = imin;
	int* offsets = BK[0];
	int remain = numBK - BUCKETS - 1;
	float y = remain / (float) n;
	int segOffset = offsets[i];
	int segNextOffset = (i == BUCKETS-1) ? n : offsets[i+1];
	int segLen = segNextOffset - segOffset;
	int blocksOffset = ((int) floor(segOffset * y)) + i + 1;
	int blocksNextOffset = ((int) floor(segNextOffset * y)) + i + 2;
	int blockLen = blocksNextOffset - blocksOffset;
	unary_fork(new RadixLoopTopDown<E,F>(A + segOffset, B + segOffset, Tmp + segOffset, BK + blocksOffset, blockLen, segLen, bits-MAX_RADIX, f),
		   new NullJob(size(1)));
      } else {
	int imid = (imin+imax)/2;
	binary_fork(new RadixLoopTopDownR<E,F>(this, imin, imid),
		    new RadixLoopTopDownR<E,F>(this, imid, imax),
		    new NullJob(size(1)));
      }
    }

      
  // Sorts the array A, which is of length n. 
  // Function f maps each element into an integer in the range [0,m)
  // If bucketOffsets is not NULL then it should be an array of length m
  // The offset in A of each bucket i in [0,m) is placed in location i
  //   such that for i < m-1, offsets[i+1]-offsets[i] gives the number 
  //   of keys=i.   For i = m-1, n-offsets[i] is the number.
  template <class E, class F>
  class iSort : public SizedJob {
    E *A; int* bucketOffsets; int n; long m; bool bottomUp; F f;    
    E *B; bIndexT *Tmp; bucketsT *BK;
    int stage;
    int *ss; 
  public:

    //stage 0 constructor
    iSort(E *A_, int* bucketOffsets_, int n_, long m_, bool bottomUp_, F f_, bool del=true) 
      : SizedJob(del), 
	A(A_), bucketOffsets(bucketOffsets_), n(n_), m(m_), bottomUp(bottomUp_), f(f_), stage(0), ss(NULL) {}
    
    //stage 1+ constructor
    iSort(iSort* from, bool del=true)
      : SizedJob(del),
	A(from->A), bucketOffsets(from->bucketOffsets), n(from->n), m(from->n), bottomUp(from->bottomUp), f(from->f), B(from->B), Tmp(from->Tmp), BK(from->BK), stage(from->stage+1), ss(from->ss)
    {}

    lluint size (const int block_size) {
      // A, B, and Tmp have size n
      // BK is about n/8
      // and bucketOffsets has size m

      return n*(2*sizeof(E) + sizeof(bIndexT) + sizeof(int)/8) + m*sizeof(int);
    }

    class ZeroBuckets : public SizedJob {
      int imin; int imax; 
      int* bo;
    public:
      ZeroBuckets(int* bo_, int imin_, int imax_, bool del=true) 
	: SizedJob(del), 
	  bo(bo_), imin(imin_), imax(imax_) {}

      lluint size (const int block_size) {

	return (imax-imin)*sizeof(int);
      }

      void function() {
	if (imax-imin < SMALLFOR_SEQ_THRESH) {
	  for (int i = imin; i<imax; i++) {
	    bo[i] = 0;
	  }
	  join();
	} else {
	  int imid = (imin+imax)/2;
	  binary_fork(new ZeroBuckets(bo,imin,imid),
		      new ZeroBuckets(bo,imid,imax),
		      new NullJob(size(1)));
	}
      }
    };

    class UpdateBucketOffsets : public SizedJob {
      int imin; int imax;
      E* A; int* bo; F f;

    public:
      UpdateBucketOffsets(E* A_, int* bo_, int imin_, int imax_, F f_, bool del=true) 
	: SizedJob(del), 
	  A(A_), bo(bo_), imin(imin_), imax(imax_), f(f_) {}

      lluint size (const int block_size) {

	return (imax-imin)*(sizeof(int)+sizeof(E));
      }
      void function() {
	if (imax-imin < SMALLFOR_SEQ_THRESH) {
	  for (int i = imin; i<imax; i++) {
	    int v = f(A[i]);
	    if (v != f(A[i+1])) bo[v] = i+1;
	  }
	  join();
	} else {
	  int imid = (imin+imax)/2;
	  binary_fork(new UpdateBucketOffsets(A,bo,imin,imid,f),
		      new UpdateBucketOffsets(A,bo,imid,imax,f),
		      new NullJob(size(1)));
	}
      }
    };
    
    void function() {
      if (stage == 0) {
	cout << "starting\n";
	int bits = utils::logUpLong(m);
	B = newA(E,n);
	Tmp = newA(bIndexT,n);
	int numBK = 1+n/(BUCKETS*8);
	BK = newA(bucketsT,numBK);
	if (bottomUp)
	  unary_fork(new RadixLoopBottomUp<E,F>(A, B, Tmp, BK, numBK, n, bits, f),
		     new iSort(this));
	else
	  unary_fork(new RadixLoopTopDown<E,F>(A, B, Tmp, BK, numBK, n, bits, f),
		     new iSort(this));
      } else if (stage == 1 && (bucketOffsets != NULL)) {
	unary_fork(new ZeroBuckets(bucketOffsets, 0, m),
		   new iSort(this));
      } else if (stage == 2) {
	unary_fork(new UpdateBucketOffsets(A, bucketOffsets, 0, n-1, f),
		   new iSort(this));
      } else if (stage == 3) {
	//ss is the result of the scan. not used anywhere, but I guess
	//we need to pass it around anyway
	ss = new int; 
	unary_fork(new Scan<int,utils::maxF<int> >(ss,bucketOffsets,bucketOffsets,m,utils::maxF<int>(),0),
		   new iSort<E,F>(this));
      } else { // stage == 4 or (stage == 1 && bucketOffsets == NULL)
	if (ss != NULL) delete ss;
	free(B); 
	free(Tmp); 
	free(BK);
	join();
      }
    }
      

    /*
    template <class E, class F>
    void iSort(E *A, int* bucketOffsets, int n, long m, F f) {
      iSort(A, bucketOffsets, n, m, false, f);}
      
    // A version that uses a NULL bucketOffset
    template <class E, class Func>
    void iSort(E *A, int n, long m, Func f) { 
      iSort(A, NULL, n, m, false, f);}

    template <class E, class F>
    void iSortBottomUp(E *A, int n, long m, F f) {
      iSort(A, NULL, n, m, true, f);}
    */
  };
};

typedef unsigned int uint;
  /*
static void integerSort(uint *A, int n) {
  long maxV = sequence::reduce(A,n,utils::maxF<uint>());
  intSort::iSort(A, NULL, n, maxV,  utils::identityF<uint>());
}

template <class T>
void integerSort(pair<uint,T> *A, int n) {
  long maxV = sequence::mapReduce<uint>(A,n,utils::maxF<uint>(),
					utils::firstF<uint,T>());
  intSort::iSort(A, NULL, n, maxV,  utils::firstF<uint,T>());
}
  */
#endif

