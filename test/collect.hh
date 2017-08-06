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


#ifndef __BUCKET_HH
#define __BUCKET_HH

long ts;
#define PAD 16

int* allocateBuckets(int numBuckets, int numBlocks) {
  return (int*) malloc(sizeof(int) * (numBuckets + PAD) * numBlocks);
}

template <class F>
class leafJob : public HR2Job {
  F f;
  int i;
public:
  leafJob(int i_, F f_) : f(f_), i(i_) {}
  lluint size (const int block_size) { return 10000;}
  lluint strand_size (const int block_size) { return(size(block_size));}
  void function() {
    f(i);
    join();
  }
};
      
template <class ftype, class ctype>
void mapIdx(int n, ftype f, ctype cont, ctype job) {
  Job **children = new Job*[n];
  for (int i=0; i < n; i++) {
    children[i] = new leafJob<ftype>(i, f);
  }
  job->fork(n, children, cont);
}

template <class btype, class F>
class countBuckets {
  btype *bucketId;
  int *counts;
  int n, numBuckets, numBlocks;
  F f;

public:
  countBuckets(btype* bucketId_, int *counts_, 
	    int n_, int numBuckets_, int numBlocks_, F f_) :
    bucketId(bucketId_), counts(counts_), 
    n(n_), numBuckets(numBuckets_), numBlocks(numBlocks_), f(f_)
  {}

  void operator() (int i) {
    long td = example_get_time();
    int blockSize = (n-1)/numBlocks + 1;
    int offset = i * blockSize;
    int size =  (i < numBlocks - 1) ? blockSize : n - offset; 
    int mycounts[numBuckets];
    //int *mycounts = newA(int,numBuckets+64);
    //int *mycounts = counts + i*(numBuckets+PAD);
    long tg = example_get_time();
    for (int j = 0; j < numBuckets; j++) 
      mycounts[j] = 0;
    for (int j = offset; j < offset + size; j++) {
      btype bucket = f(j);
      bucketId[j] = bucket;
      mycounts[bucket]++;
    }
    for (int j = 0; j < numBuckets; j++) 
      counts[j + i*(numBuckets+PAD)] = mycounts[j];
    //free(mycounts);
    //printf("%d: %d, %d, %d\n",i,(int) (td-ts), 
	   //(int) (tg-td), (int) (example_get_time()-tg));
    return;
  }
};

template <class E, class btype>
class relocateBuckets {
  E *A, *B;
  btype *bucketId;
  int *counts;
  int n, numBuckets, numBlocks;

public:
  relocateBuckets(E *A_, E *B_, btype* bucketId_,  int *counts_,
	   int n_, int numBuckets_, int numBlocks_) :
    A(A_), B(B_), bucketId(bucketId_), counts(counts_), 
    n(n_), numBuckets(numBuckets_), numBlocks(numBlocks_)
  {}

  void operator() (int i) {
    int blockSize = (n-1)/numBlocks + 1;
    int offset = i * blockSize;
    int size =  (i < numBlocks - 1) ? blockSize : n - offset; 
    int mycounts[numBuckets];
    //int *mycounts = newA(int,numBuckets+64);
    for (int j = 0; j < numBuckets; j++) 
      mycounts[j] = counts[j + i*(numBuckets+PAD)];
    for (int j = offset; j < offset + size; j++) 
      B[mycounts[bucketId[j]]++] = A[j];
    //free(mycounts);
  }
};

void scanBuckets(int* counts, int numBuckets, int numBlocks, 
		 int* bucketStarts) {
  int sum = 0;
  for (int i = 0; i < numBuckets; i++)
    for (int j = 0; j < numBlocks; j++) {
      int v = counts[j*(numBuckets+PAD)+i];
      counts[j*(numBuckets+PAD)+i] = sum;
      sum += v;
    }

  //int* bucketStarts = newA(int,numBuckets+1);
  for (int i = 0; i < numBuckets; i++)
    bucketStarts[i] = counts[i];
  bucketStarts[numBuckets] = sum;
  //return bucketStarts;
}

template <class E, class btype, class F>
class Collect : public HR2Job {
  E *A, *B; 
  int n;
  F f;
  int numBlocks, numBuckets;
  int *counts;
  btype *bucketId;
  int *bucketStarts;
  int stage;
  bool freeCounts;
  
public:
  Collect (E* A_, E* B_, int n_, int numBuckets_, btype* bucketId_,  
	   int* bucketStarts_, F f_, int *counts_=NULL, int numBlocks_=-1, 
	   bool del=true)
    : HR2Job (del), A(A_), B(B_), counts(counts_), bucketId(bucketId_),
      numBlocks(numBlocks_), numBuckets(numBuckets_), n(n_), f(f_), 
      bucketStarts(bucketStarts_), stage(0), freeCounts(0) {}

  Collect (Collect *from, bool del=true)
    : HR2Job (del), 
      A(from->A), B(from->B), 
      n(from->n), 
      f(from->f), 
      freeCounts(from->freeCounts), 
      numBlocks(from->numBlocks),
      numBuckets(from->numBuckets), 
      counts(from->counts),
      bucketId(from->bucketId),
      bucketStarts(from->bucketStarts),
      stage(from->stage+1) 
  {}

  lluint size (const int block_size) { 
    return n*(2*sizeof(E)+sizeof(btype)); }

  lluint strand_size (const int block_size) { 
    return size(block_size); }
  
  void function () {
    if (stage == 0) {
      if (numBlocks<0) numBlocks = min(126,1+n/20000);
      if (counts==NULL) {
	counts = allocateBuckets(numBuckets,numBlocks);
	freeCounts = 1;
      }
      mapIdx(numBlocks, 
	     countBuckets<btype,F>(bucketId,counts,n,numBuckets,numBlocks,f),
	     new Collect(this), this);

    } else if (stage == 1) {
      scanBuckets(counts, numBuckets, numBlocks, bucketStarts);
      mapIdx(numBlocks,
	     relocateBuckets<E,btype>(A,B,bucketId,counts,n,numBuckets,numBlocks),
	     new Collect(this), this);

    } else if (stage == 2) {
      //free(bucketStarts);
      if (freeCounts) free(counts);
      join();
    }
  }
};

#endif

