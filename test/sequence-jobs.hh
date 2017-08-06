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

// -*- C++ -*-

#ifndef A_SEQUENCE_INCLUDED
#define A_SEQUENCE_INCLUDED

#include <sched.h>
#include <iostream>
#include "utils.hh"
#include "Job.hh"
#include "recursion_basecase.hh"
#include "common.hh"

using namespace std;

template <class E>
class GatherScatter: public HR2Job {
  E** A; E** B; int n;
  int stage;
  lluint tsize, strandsize;

#define GS_PAR_THRESHOLD (1<<12)
public:
  GatherScatter(E **A_, E **B_, int n_, int stage_=0, bool del=true)
    : HR2Job (del), A(A_), B(B_), n(n_), stage(stage_), tsize(0), strandsize(0) {}

  lluint size (const int block_size) {
    if (tsize != 0) return tsize;
    tsize =  2*round_up(n*sizeof(E*), block_size);
    if (sizeof(E)>block_size)
      tsize += 2*n*sizeof(E);
    else 
      tsize += 2*n*block_size;
    return tsize;
  }

  lluint strand_size (const int block_size) {
    if (strandsize != 0) return strandsize;
    if (stage == 0) {
      if (n < GS_PAR_THRESHOLD)
	    strandsize = size (block_size);
      else
	strandsize = STRAND_SIZE;
    } else {
      strandsize = STRAND_SIZE;
    }
     return strandsize;
  }
  
  void function () {
    if (stage == 0) {
      if (n < GS_PAR_THRESHOLD) {
	for (int i=0; i<n; ++i)
	  *B[i] = *A[i];
	join();
      } else {
#define BALANCE (0.8)
	int cut = BALANCE*n;
	binary_fork (new GatherScatter<E> (A, B, cut),
		     new GatherScatter<E> (A+cut, B+cut, n-cut),
		     new GatherScatter<E> (A, B, n, 1));
      }
    } else {
      join();
    }
  }
};

template <class E>
class IncrementArrayRef: public SizedJob {
  E** A; int n;
public:
  IncrementArrayRef(E **A_, int n_, bool del=true)
    : SizedJob (del), A(A_), n(n_) {}

  lluint size (const int block_size) {
    return 2*round_up(n*sizeof(E*), block_size);
  }
  
  void function () {
    for (int i=0; i<n; ++i) 
      (*A[i])++;
    join ();
  }
};

template <class E>
class Gather: public SizedJob {
  E** A;  int n;
public:
  Gather (E **A_, int n_, bool del=true)
    : SizedJob (del), A(A_), n(n_) {}

  lluint size (const int block_size) {
    return 2*round_up(n*sizeof(E*), block_size);
  }
  
  void function () {
    int sum;
    for (int i=0; i<n; ++i) {
      sum += *A[i];
    }
    int* sum_ptr = new int;
    *sum_ptr = sum;
    join ();
  }
};


template <class E>
class Scatter: public SizedJob {
  E** A;  int n;
public:
  Scatter (E **A_, int n_, bool del=true)
    : SizedJob (del), A(A_), n(n_) {}

  lluint size (const int block_size) {
    return 2*round_up(n*sizeof(E*), block_size);
  }
  
  void function () {
    for (int i=0; i<n; ++i) 
      *A[i] = i;
    join ();
  }
};
template <class AT, class BT, class F>
class Empty : public HR2Job {
  AT* A; BT* B; int n; F f;
  int stage;

public:
  Empty (AT *A_, BT *B_, int n_, F f_, int stage_=0,
       bool del=true)
    : HR2Job (del),
      A(A_), B(B_), n(n_), f(f_), stage(stage_)  {}

  lluint size (const int block_size) {
    return round_up(n*sizeof(AT), block_size) + round_up(n*sizeof(BT), block_size);
  }

  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size (block_size);
    } else { 
      if (n > _SCAN_BSIZE)
	return STRAND_SIZE;
      else 
	return size (block_size);
    }
  }
  
  void function () {
    if (stage == 0) {
      if (n<_SCAN_BSIZE/4) {
	for (volatile int i=0; i<n; ++i)
	  B[0] += f(A[0]);
	join ();
      } else {
	binary_fork (new Empty<AT,BT,F> (A,B,n/3,f),
		     new Empty<AT,BT,F> (A+n/3,B+n/3,n-n/3,f),
		     new Empty<AT,BT,F> (A,B,n,f,1));
      }
    } else {
      join ();
    }
  }
};

template <class AT, class BT, class F>
class Map : public HR2Job {
  AT* A; BT* B; int n; F f;
  int stage;

public:
  Map (AT *A_, BT *B_, int n_, F f_, int stage_=0,
       bool del=true)
    : HR2Job (del),
      A(A_), B(B_), n(n_), f(f_), stage(stage_)  {}

  lluint size (const int block_size) {
    return round_up(n*sizeof(AT), block_size) + round_up(n*sizeof(BT), block_size);
  }

  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size (block_size);
    } else { 
      if (n > _SCAN_BSIZE)
	return STRAND_SIZE;
      else 
	return size (block_size);
    }
  }
  
  void function () {
    if (stage == 0) {
      if (n<_SCAN_BSIZE) {
	for (int i=0; i<n; ++i)
	  B[i] = f(A[i]);
	join ();
      } else {
	binary_fork (new Map<AT,BT,F> (A,B,n/2,f),
		     new Map<AT,BT,F> (A+n/2,B+n/2,n-n/2,f),
		     new Map<AT,BT,F> (A,B,n,f,1));
      }
    } else {
      join ();
    }
  }
};

template <class AT, class F>
class MapInt : public HR2Job {

  AT* A; int s,e; F f;
  int stage;

public:
  MapInt (AT* A_, int n_, F f_, int stage_=0, bool del=true)
    : HR2Job (del), A(A_), s(0), e(n_), f(f_), stage(stage_) {}
  
  MapInt (AT* A_, int s_, int e_, F f_, int stage_=0, bool del=true)
    : HR2Job (del), A(A_), s(s_), e(e_), f(f_), stage(stage_) {}

  lluint size (const int block_size) {
    return round_up((e-s)*sizeof(AT), block_size);
  }

  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size (block_size);
    } else { 
      if (e-s > _SCAN_BSIZE)
	return STRAND_SIZE;
      else 
	return size (block_size);
    }
  }
  
  void function () {
    if (stage == 0) {
      if (e-s<_SCAN_BSIZE) {
	for (int i=s; i<e; ++i)
	  A[i] = f(i);
	join ();
      } else {
	int mid=(e+s)/2;
	binary_fork (new MapInt<AT,F> (A,s,mid,f),
		     new MapInt<AT,F> (A,mid,e,f),
		     new MapInt<AT,F> (A,s,e,f,1));
      }
    } else {
      join ();
    }
  }
};

template <class ET, class F>
class ScanUpRCont : public HR2Job {

  F f;
  ET* Sums;
  ET* result; ET* result1;ET* result2;
  int s,e;

public:
  ScanUpRCont(ET* result_, ET* result1_, ET* result2_, ET* Sums_, F f_,
	      int s_, int e_,
	      bool del=true)
    : HR2Job (del),
      Sums (Sums_), f(f_), s(s_), e(e_),
      result(result_), result1(result1_), result2(result2_)
    {}

  lluint size (const int block_size) {
    return round_up((e-s)*sizeof(ET), block_size)
      + round_up (((e-s)/_SCAN_BSIZE)*sizeof(ET), block_size);;
  }
  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE ==1) {
      return size(block_size);
    } else {
      return STRAND_SIZE;
    }
  }

  void function () {
    *Sums = *result1;
    *result = f (*result1, *result2);
    //delete result1, result2;
    join ();
  }
};

template <class ET, class F, class G>
class ScanUpR : public HR2Job {

  ET* result;
  ET* Sums; int s; int e; F f; G g;
   
public:
  ScanUpR(ET* result_, ET* Sums_, int s_, int e_, F f_, G g_,
	  bool del=true)
    : HR2Job (del),
      result (result_),
      Sums(Sums_), s(s_), e(e_), f(f_), g(g_)
    {}

  lluint size (const int block_size) {
    return round_up((e-s)*sizeof(ET), block_size)
          + round_up (((e-s)/_SCAN_BSIZE)*sizeof(ET), block_size);
  }
  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size(block_size);
    } else {
      if (e-s>_SCAN_BSIZE)
	return size(block_size);
      else 
	return STRAND_SIZE;
    }
  }
  
  void function () {
    int n = e-s;
    if (n > _SCAN_BSIZE) {
      int nl = _nextPow(n>>1);
      int m = s + nl;
      ET ls, rs;
      ET* result1= new ET; ET* result2= new ET;
      Job *child0 = new ScanUpR<ET,F,G> (result1, Sums+1,                    s,m,f,g);
      Job *child1 = new ScanUpR<ET,F,G> (result2, Sums+(nl>>_SCAN_LOG_BSIZE),m,e,f,g);
      binary_fork (child0, child1,
		   new ScanUpRCont<ET,F> (result,
					  static_cast<ScanUpR*>(child0)->result,
					  static_cast<ScanUpR*>(child1)->result,
					  Sums, f,s,e));
    } else {
      ET r = g(s);
      for (int i=s+1; i < e; i++) r = f(r,g(i));
      *result = r;
      join();
    }
  }
};

template <class ET, class F, class G>
class ScanDownR : public HR2Job {

  ET *Out; ET* Sums; int s; int e; ET v; F f; G g;
  int stage;
public:
  // STage 0 constructor
  ScanDownR(ET *Out_, ET* Sums_, int s_, int e_, ET v_, F f_, G g_,
	    bool del=true)
    : HR2Job (del),
      Out(Out_), Sums (Sums_), s(s_), e(e_), v(v_), f(f_), g(g_),
      stage(0)
    {}

  // STage 1 constructor
  ScanDownR(int s_, int e_, G g_,
	    bool del=true)
    : HR2Job (del),
      s(s_), e(e_), g(g_),
      stage(1)
    {}

  lluint size (const int block_size) {
    return round_up((e-s)*sizeof (ET), block_size)
      + round_up (((e-s)/_SCAN_BSIZE)*sizeof(ET), block_size);
  }
  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size(block_size);
    } else {
      if (e-s>_SCAN_BSIZE)
	return size(block_size);
      else 
	return STRAND_SIZE;
    }
  }
  
  void function () {
    if (stage==0) {
      int n = e-s;
      if (n > _SCAN_BSIZE) {
	int nl = _nextPow(n>>1);
	int m = s + nl;
	binary_fork (new ScanDownR<ET,F,G > (Out,Sums+1,                    s,m,v,         f,g),
		     new ScanDownR<ET,F,G > (Out,Sums+(nl>>_SCAN_LOG_BSIZE),m,e,f(v,*Sums),f,g),
		     new ScanDownR<ET,F,G > (s,e,g));
      } else {
	ET r = v;
	for (int i = s; i < e-1; i++) {
	  ET t = g(i);
	  Out[i] = r;
	  r = f(r,t);
	}
	Out[e-1] = r;
	join ();
      }
    } else {
      join ();
    }
  }
};

template <class ET, class F> 
class Scan : public HR2Job {
  
  ET* result; ET *In; ET* Out; int n; F f; ET zero;
  int stage;
  ET *Sums;
  ET *s;
  
public:
  
  Scan(ET* result_, ET *In_, ET* Out_, int n_, F f_, ET zero_,
       int stage_=0, bool del=true)
    : HR2Job (del),
      result (result_), In(In_), Out(Out_), n(n_), f(f_), zero(zero_),
      stage (stage_)
    {}

  Scan (Scan *from, int stage_, bool del=true)
    : HR2Job (del),
      result (from->result),
      In (from->In), Out (from->Out), n(from->n), f(from->f), zero(from->zero),
      Sums(from->Sums), s(from->s),
      stage (stage_)
    {}

  lluint size (const int block_size) {
    return 2*round_up (n*sizeof(ET), block_size)
      + round_up ((n/_SCAN_BSIZE)*sizeof(ET), block_size);
  }
  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size(block_size);
    } else {
      return STRAND_SIZE_MODE;
    }
  }
  
  void function () {
    if (stage==0) {
      Sums = newA(ET,1+n/_SCAN_BSIZE);
      s = newA(ET,1);
      unary_fork (new ScanUpR<ET,F,getA<ET> >(s,Sums,0,n,f,getA<ET>(In),false),
		  new Scan<ET,F> (this, 1));
    } else if (stage == 1) {
      unary_fork (new ScanDownR<ET,F,getA<ET> >(Out,Sums,0,n,zero,f,getA<ET>(In)),
		  new Scan<ET,F> (this, 2));
    } else {
      delete Sums;
      *result = f(zero,*s);
      join();
    }
  }
};

class JobGenerator {
public:
  virtual HR2Job* operator()(int i)=0;
};

class Pfor : public HR2Job {

  HR2Job** A;               // An array of jobs
  HR2Job*** A_ptr;
  JobGenerator *gen;
  int n;
  lluint* sizes, *size_sums;
  int stage;
  int s,e;
  lluint **size_sums_ptr;

#define ST_SIZE 10
#define ST_SIZE_SUMS 11
#define ST_SIZE_JOIN 12
#define ST_GEN_JOBS 13
#define ST_GEN_JOBS_RECURSIVE 14
#define ST_GEN_JOBS_RECURSIVE_JOIN 15
  
public:
  
  Pfor (HR2Job**A_, int n_, lluint* size_sums_ = NULL, int stage_=0,
       bool del=true)
    : HR2Job (del),
      A(A_), n(n_), size_sums(size_sums_), stage(stage_),  s(0), e(n_)
    { }

  Pfor (HR2Job**A_, int n_, lluint *sizes_, lluint* size_sums_, lluint **size_sums_ptr_,
	int stage_=ST_SIZE,
	bool del=true)
    : HR2Job (del),
      A(A_), s(0), e(n_), n(n_), sizes(sizes_), size_sums(size_sums_),
      size_sums_ptr (size_sums_ptr_),
      stage(stage_)  
    { }
  
  Pfor (class JobGenerator *gen_, int n_, HR2Job **A_=NULL, int stage_=ST_GEN_JOBS,
	HR2Job ***A_ptr_=NULL, lluint **size_sums_ptr_=NULL,
	bool del=true)
    : HR2Job (del), gen(gen_), A(A_),
      s(0), e(n_), size_sums_ptr (size_sums_ptr_), A_ptr(A_ptr_),
      stage(stage_) 
    {}
  
  Pfor (class JobGenerator *gen_, int s_, int e_, HR2Job **A_=NULL, int stage_=ST_GEN_JOBS,
	bool del=true)
    : HR2Job (del), gen(gen_), A(A_),
      s(s_), e(e_), 
      stage(stage_)
    {}
  
  // Cant support multiple block sizes!!
  // Size should be same across stages.
  lluint size (const int block_size) {
    switch (stage) {
    case 0:
    case 1:
      if (size_sums == NULL) {
	std::cerr<<"Jobs should be sized before calling Pfor"<<std::endl;
	exit(-1);
      } else {
	return size_sums[e]-size_sums[s];
      }
    case ST_GEN_JOBS:
    case ST_GEN_JOBS_RECURSIVE:
    case ST_GEN_JOBS_RECURSIVE_JOIN:
    case ST_SIZE:
    case ST_SIZE_SUMS:
    case ST_SIZE_JOIN:
      return 2*round_up ((e-s)*sizeof(lluint), block_size)
	+ round_up (((e-s)/_SCAN_BSIZE)*sizeof(lluint), block_size)
	+ round_up ((e-s)*sizeof(HR2Job*), block_size);
    }
  }
  lluint strand_size (const int block_size) {
    return size(block_size);
  }
  
  void function () {
    //std::cout<<"Pfor: "<<stage<<" "<<s<<" "<<e<<std::endl;
    if (stage == 0) {
      if (n==1) {
	unary_fork (*A, new Pfor (A,n,size_sums,1));
      } else {
	binary_fork (new Pfor (A,     n/2,   size_sums,     0),
		     new Pfor (A+n/2, n-n/2, size_sums+n/2, 0),
		     new Pfor (A,n,size_sums,1));
      }

    } else if (stage == 1) {
      join();

    } else if (stage == ST_GEN_JOBS) {
      *A_ptr = A = new HR2Job*[e-s];
      binary_fork (new Pfor (gen, s, (s+e)/2, A, ST_GEN_JOBS_RECURSIVE),
		   new Pfor (gen, (s+e)/2, e, A, ST_GEN_JOBS_RECURSIVE),
		   new Pfor (A, e-s, NULL, NULL, size_sums_ptr, ST_SIZE));      

    } else if (stage == ST_GEN_JOBS_RECURSIVE) {
      if (e-s<_SCAN_BSIZE) {
	for (int i=s; i<e; ++i)
	  A[i] = (*gen)(i);
	join ();
      } else {
	binary_fork (new Pfor (gen, s, (s+e)/2, A, ST_GEN_JOBS_RECURSIVE),
		     new Pfor (gen, (s+e)/2, e, A, ST_GEN_JOBS_RECURSIVE),
		     new Pfor (gen, s, e, A, ST_GEN_JOBS_RECURSIVE_JOIN));      
      }
      
    } else if (stage == ST_GEN_JOBS_RECURSIVE_JOIN) {
      join ();
      
    } else if (stage == ST_SIZE) {
      if (cast(*A, false)!=NULL) {
	sizes = new lluint[n+1];
	sizes[n]=0;
	unary_fork (new Map<HR2Job*, lluint, HR2Job::Size> (A,sizes,n, HR2Job::Size()),
		    new Pfor (A,n,sizes,NULL,size_sums_ptr,ST_SIZE_SUMS));
	return;
      } else {
	join();
      }
    } else if (stage == ST_SIZE_SUMS) {
      *size_sums_ptr=size_sums = new lluint[n+1]; size_sums[n]=0;
      unary_fork (new Scan<lluint, plus<lluint> > (new lluint,sizes,size_sums,n+1,plus<lluint>(),0),
		  new Pfor (A,n,sizes,size_sums,size_sums_ptr,ST_SIZE_JOIN));
    } else if (stage == ST_SIZE_JOIN) {
      join();
    }
  }
};

template <class E>
class RecursiveGatherScatter: public HR2Job {
  E** A; E** B; int n;
  int num_repeats;
  int stage;

  #define BOTTOM_THRESHOLD (1<<15)
  #define CUT_RATIO (0.5)
    
public:
  RecursiveGatherScatter(E **A_, E **B_, int n_, int num_repeats_,
			 int stage_=0, bool del=true)
    : HR2Job (del), A(A_), B(B_), n(n_),
      num_repeats(num_repeats_), stage (stage_) {}

  lluint size (const int block_size) {
    return 2*round_up(n*sizeof(E*), block_size)
      + 2*n;
    // Size not devided by block size because
    // each entry is a diferent block
  }
  
  lluint strand_size (const int block_size) {
    return size(block_size);
  }
  
  void function () {
    if (n < BOTTOM_THRESHOLD) {
      join();
    } else {
      if (stage < num_repeats) {
	unary_fork (new GatherScatter<E> (A, B, n),
		    new RecursiveGatherScatter (A, B, n, num_repeats, 1+stage));

      } else if (stage == num_repeats) {
	long int cut = (int)(CUT_RATIO * n);
	binary_fork ( new RecursiveGatherScatter (A, B, cut, num_repeats),
		      new RecursiveGatherScatter (A+cut, B+cut, n-cut, num_repeats),
		      new RecursiveGatherScatter (A, B, n, num_repeats, 1+num_repeats) );
      } else {
	join ();
      }
    }
  }
};

template <class OT, class F, class G> 
class Reduce : public HR2Job {
  int s,e; F f; G g; OT* result; OT *v1,*v2;int stage;

public: 
  Reduce (OT* result_, int s_, int e_, F f_, G g_, OT *v1_=NULL, OT *v2_=NULL,
	  int stage_=0, bool del=true)
    : HR2Job(del), result(result_),  //result must be allocated to size 4*n/SCAN_BSIZE and passed
      s(s_), e(e_), f(f_), g(g_), v1(v1_), v2(v2_),
      stage(stage_) {}

  lluint size (const int block_size) {return 0;}
  lluint strand_size (const int block_size) {return 0;}

  void function () {
    assert (result!=NULL);
    //printf("%d %d %d\n",s,e,stage);
    if (stage==0) {
      if ((e-s) <= _SCAN_BSIZE) {
	*result = g(s);
	for (int i=s+1; i<e; i++) *result = f(*result,g(i));
	join();
      } else {
	int m = (s+e)/2;
	//v1=result+1; v2=result+1+m/_SCAN_BSIZE;
	v1=newA(OT,1);v2=newA(OT,1);
	binary_fork (new Reduce<OT,F,G>(v1,s,m,f,g),
		     new Reduce<OT,F,G>(v2,m,e,f,g),
		     new Reduce<OT,F,G>(result,s,e,f,g,v1,v2,1));
      }
    } else if (stage==1) {
      *result = f(*v1,*v2);
      join();
    } else {
      fprintf(stderr,"Invalid stage number\n");
    }
  }
};

template <class PRED> 
class FilterUpR : public HR2Job {
  bool *Fl; int* Sums; int s, e; PRED f; 
  int* result; int stage; int *v1;int *v2;

public: 
  FilterUpR(int* result_, bool *Fl_, int* Sums_, int s_,int e_, PRED f_,
	    int stage_=0, int* v1_=NULL, int* v2_=NULL, bool del=true)
    : HR2Job (del),
      result(result_), Fl(Fl_), Sums(Sums_), s(s_), e(e_), f(f_),
      v1(v1_),v2(v2_),stage(stage_) {}

  lluint size (const int block_size) {exit(-1);}
  lluint strand_size (const int block_size) {exit(-1);}
  
  void function() {
    int n = e-s;
    printf("Sums: %p, n: %d\n", Sums, n);
    if (stage=0) {
      if (n < _SCAN_BSIZE) {
	int v = 0;
	for (int i=s; i < e; i++) v += (Fl[i] = f(i));
	*result=v;
	join();
      } else {
	int nl = _nextPow(n>>1);
	int m = s + nl;
	v1 = newA(int,1); v2=newA(int,1);
	printf("stage:%d n:%d Sums:%p %p v1:%p v2: %p\n",stage,n,Sums,Sums+(n>>_SCAN_LOG_BSIZE),v1,v2);
	binary_fork(new FilterUpR<PRED>(v1,Fl,Sums+1,                    s,m,f),
		    new FilterUpR<PRED>(v2,Fl,Sums+(n>>_SCAN_LOG_BSIZE),m,e,f),
		    new FilterUpR<PRED>(result,Fl,Sums,s,e,f,1,v1,v2) );
      } 
    } else {
      printf("stage:%d n:%d Sums:%p %p v1:%p v2: %p\n",stage,n,Sums,Sums+(n>>_SCAN_LOG_BSIZE),v1,v2);
      *Sums = *v1;
      *result = *v1+*v2;
      join();
    }
  }
};

template <class ET, class F>
class FilterDownR : public HR2Job {
  
  bool *Fl; ET* Out; int* Sums;
  int s,e; int v; F f; int stage;
  
public:
  FilterDownR (bool *Fl_, ET* Out_, int* Sums_, int s_, int e_, int v_, F f_,
	       int stage_=0, bool del=false) 
    : HR2Job(del),
      Fl(Fl_), Out(Out_), Sums(Sums_), s(s_), e(e_), v(v_), f(f_), 
      stage(stage_) {}
  
  lluint size (const int block_size) {exit(-1);}
  lluint strand_size (const int block_size) {exit(-1);}
  
  void function ()  {
    int n = e-s;
    if (stage==0) {
      if (n < _SCAN_BSIZE) {
	int j = v;
	for (int i=s; i<e; i++) 
	  if (Fl[i]) Out[j++] = f(i);
	join();
      } else {
	int nl = _nextPow(n>>1);
	int m = s + nl;
	printf("%d %p %p\n",n,Sums,Sums+(n>>_SCAN_LOG_BSIZE));
	binary_fork(new FilterDownR<ET,F>(Fl,Out,Sums+1,             s,m,v      ,f),
		    new FilterDownR<ET,F>(Fl,Out,Sums+(n>>_SCAN_LOG_BSIZE),m,e,v+*Sums,f),
		    new FilterDownR<ET,F>(Fl,Out,Sums,s,e,v,f,1));
      } 
    } else {
      join();
    }
  }
};

template <class OT, class PRED, class F> 
class Filter : public HR2Job {

  OT* Out; int s; int e; PRED p; F f; int* Sums; bool *Fl;
  int stage;  int *result;
  
public:
  Filter (int *result_, OT* Out_, int s_, int e_, PRED p_, F f_,
	  int stage_=0, int* Sums_=NULL, bool* Fl_=NULL, bool del=false)
    : HR2Job(del), Out(Out_), s(s_), e(e_), p(p_), f(f_),
      result(result_), Sums(Sums_), Fl(Fl_), stage(stage_)  {}

  lluint size (const int block_size) {exit(-1);}
  lluint strand_size (const int block_size) {exit(-1);}
  
  void function() {
    int n = e-s;
    if (stage == 0) {
      Sums = newA(int,1+4*n/_SCAN_BSIZE);
      printf("Sums: %p, %p\n", Sums, Sums+1+4*n/_SCAN_BSIZE);
      bool *Fl = newA(bool,n);
      unary_fork (new FilterUpR<PRED>(result,Fl,Sums,s,e,p),
		  new Filter<OT,PRED,F>(result,Out,s,e,p,f,1));
    } else if (stage == 1) {
      unary_fork (new FilterDownR<OT,F>(Fl,Out,Sums,s,e,0,f),
		  new Filter<OT,PRED,F>(result,Out,s,e,p,f,2));
    } else {
      delete Sums; delete Fl;
      join();
    }
  }
};

template <class ET> 
class Pack : public HR2Job {
  ET* In; ET* Out; bool* Fl; int n; int *Sums;
  int *result; int stage;
public:
  Pack(int *result_, ET* In_, ET* Out_, bool* Fl_, int n_,
       int stage_=0, int* Sums_=NULL, bool del=false)
    : HR2Job(del), In(In_), Out(Out_), Fl(Fl_), n(n_),
      stage(stage_), Sums(Sums_), result(result_) {}  

  lluint size (const int block_size) {exit(-1);}
  lluint strand_size (const int block_size) {exit(-1);}

  void function () {
    if (stage==0) {
      Sums = newA(int,1+(4*(n>>_SCAN_LOG_BSIZE)));
      unary_fork (new FilterUpR<getA<bool> >(result,Fl,Sums,0,n,getA<bool>(Fl)),
		  new Pack(result,In,Out,Fl,n,1,Sums));
    } else if (stage==1) {
      unary_fork (new FilterDownR<ET, getA<ET> >(Fl,Out,Sums,0,n,0,getA<ET>(In)),
		  new Pack(result,In,Out,Fl,n,2,Sums));
    } else if (stage==2) {
      delete Sums;
      join();
    }
  }
};

/*
namespace sequence {
  static int _nextPow(int i) {
    int a=0;
    int b=i-1;
    while (b > 0) {b = b >> 1; a++;}
    return (1 << a);
  }

  template <class ET>
  struct getA {
    ET* A;
    getA(ET* AA) : A(AA) {}
    ET operator() (int i) {return A[i];}
  };

  template <class IT, class OT, class F>
  struct getAF {
    IT* A;
    F f;
    getAF(IT* AA, F ff) : A(AA), f(ff) {}
    OT operator () (int i) {return f(A[i]);}
  };

  // g is the map function (applied to each element)
  // f is the reduce function
  // need to specify OT since it is not an argument
  template <class OT, class IT, class F, class G> 
  OT mapReduce(IT* A, int n, F f, G g) {
    return reduce<OT>(0,n,f,getAF<IT,OT,G>(A,g));
  }

  template <class ET, class F, class G> 
  pair<ET,int> maxIndex_(int s, int e, F f, G g) {
    if ((e-s) > _BSIZE) {
      pair<ET,int> v1,v2;
      int m = (s+e)/2;
      v1 = cilk_spawn maxIndex_<ET,F,G>(s,m,f,g);
      v2 = maxIndex_<ET,F,G>(m,e,f,g);
      cilk_sync;
      return f(v1.first,v2.first) ? v1 : v2;
    } else {
      pair<ET,int> r(g(s),s);
      for (int i=s+1; i < e; i++) {
	ET v = g(i);
	if (f(v,r.first)) {
	  r.first = v;
	  r.second = i;
	}
      }
      return r;
    }
  }

  template <class ET, class F, class G> 
  int maxIndex(int s, int e, F f, G g) {
    return maxIndex_<ET,F,G>(s, e, f, g).second;
  }

  template <class ET, class F> 
  int maxIndex(ET* A, int n, F f) {
    return maxIndex_<ET,F,getA<ET> >(0, n, f, getA<ET>(A)).second;
  }

  template <class ET, class F, class G>
  ET scanUpR(ET* Sums, int s, int e, F f, G g) {
    int n = e-s;
    if (n > _SCAN_BSIZE) {
      int nl = _nextPow(n>>1);
      int m = s + nl;
      ET ls, rs;
      ls = cilk_spawn scanUpR(Sums+1,                    s,m,f,g);
      rs =            scanUpR(Sums+(nl>>_SCAN_LOG_BSIZE),m,e,f,g);
      cilk_sync;
      *Sums = ls;
      return f(ls,rs);
    } else {
      ET r = g(s);
      for (int i=s+1; i < e; i++) r = f(r,g(i));
      return r;
    }
  }

  template <class ET, class F, class G> 
  void scanDownR(ET *Out, ET* Sums, int s, int e, ET v, F f, G g) {
    int n = e-s;
    if (n > _SCAN_BSIZE) {
      int nl = _nextPow(n>>1);
      int m = s + nl;
      cilk_spawn 
      scanDownR(Out,Sums+1,             s,m,v,         f,g);
      scanDownR(Out,Sums+(nl>>_SCAN_LOG_BSIZE),m,e,f(v,*Sums),f,g);
      cilk_sync;
    } else {
      ET r = v;
      for (int i = s; i < e-1; i++) {
	ET t = g(i);
	Out[i] = r;
	r = f(r,t);
      }
      Out[e-1] = r;
    }
  }

  template <class ET, class F, class G> 
  void scanDownIR(ET *Out, ET* Sums, int s, int e, ET v, F f, G g) {
    int n = e-s;
    if (n > _SCAN_BSIZE) {
      int nl = _nextPow(n>>1);
      int m = s + nl;
      cilk_spawn 
      scanDownIR(Out,Sums+1,             s,m,v,         f,g);
      scanDownIR(Out,Sums+nl/_SCAN_BSIZE,m,e,f(v,*Sums),f,g);
      cilk_sync;
    } else {
      ET r = v;
      for (int i = s; i < e; i++) {
	r = f(r,g(i));
	Out[i] = r;
      }
    }
  }

  template <class ET, class F> 
  ET scan(ET *In, ET* Out, int n, F f, ET zero) {
    ET *Sums = newA(ET,1+n/_SCAN_BSIZE);
    ET s = scanUpR(Sums,0,n,f,getA<ET>(In));
    scanDownR(Out,Sums,0,n,zero,f,getA<ET>(In));
    delete Sums;
    return f(zero,s);
  }

  template <class ET, class F> 
  ET scanI(ET *In, ET* Out, int n, F f, ET zero) {
    ET *Sums = newA(ET,1+n/_SCAN_BSIZE);
    ET s = scanUpR(Sums,0,n,f,getA<ET>(In));
    scanDownIR(Out,Sums,0,n,zero,f,getA<ET>(In));
    delete Sums;
    return f(zero,s);
  }

  template <class PRED> 
  int filterUpR(bool *Fl, int* Sums, int s, int e, PRED f) {
    int n = e-s;
    if (n > _SCAN_BSIZE) {
      int nl = _nextPow(n>>1);
      int m = s + nl;
      int ls,rs;
      ls = cilk_spawn 
	   filterUpR<PRED>(Fl,Sums+1,                    s, m, f);
      rs = filterUpR<PRED>(Fl,Sums+(nl>>_SCAN_LOG_BSIZE),m, e, f);
      cilk_sync;
      *Sums = ls;
      return ls+rs;
    } else {
      int v = 0;
      for (int i=s; i < e; i++) v += (Fl[i] = f(i));
      return v;
    }
  }

  template <class ET, class F>
  void filterDownR(bool *Fl, ET* Out, int* Sums, int s, int e, int v, F f) {
    int n = e-s;
    if (n > _SCAN_BSIZE) {
      int nl = _nextPow(n>>1);
      int m = s + nl;
      cilk_spawn
      filterDownR<ET,F>(Fl,Out,Sums+1,             s,m,v      ,f);
      filterDownR<ET,F>(Fl,Out,Sums+nl/_SCAN_BSIZE,m,e,v+*Sums,f);
      cilk_sync;
    } else {
      int j = v;
      for (int i=s; i < e; i++) 
	if (Fl[i]) Out[j++] = f(i);
    }
  }

  template <class ET, class G>
  void unfilterDownR(bool *Fl, ET* In, ET* Out, int* Sums, int s,int e,int v) {
    int n = e-s;
    if (n > _SCAN_BSIZE) {
      int nl = _nextPow(n>>1);
      int m = s + nl;
      cilk_spawn
      unfilterDownR<ET,G>(Fl,In,Out,Sums+1,             s,m,v      );
      unfilterDownR<ET,G>(Fl,In,Out,Sums+nl/_SCAN_BSIZE,m,e,v+*Sums);
      cilk_sync;
    } else {
      int j = v;
      for (int i=s; i < e; i++) 
	if (Fl[i]) Out[i] = In[j++];
    }
  }

  template <class OT, class PRED, class F> 
  int filter(OT* Out, int s, int e, PRED p, F f) {
    int n = e-s;
    int *Sums = newA(int,1+n/_SCAN_BSIZE);
    bool *Fl = newA(bool,n);
    int m = filterUpR<PRED>(Fl,Sums,s,e,p);
    filterDownR<OT,F>(Fl,Out,Sums,s,e,0,f);
    delete Sums;
    delete Fl;
    return m;
  }

  template <class ET, class PRED> 
  int filter(ET* In, ET* Out, int n, PRED p) {
    return filter(Out,0,n,getAF<ET,bool,PRED>(In,p),getA<ET>(In));
  }

  template <class PRED>
  int packIndex(int* Out, int s, int e, PRED p) {
    return filter(Out,s,e,p,utils::identityF<int>());
  }

  template <class ET> 
  int pack(ET* In, ET* Out, bool* Fl, int n) {
    int *Sums = newA(int,1+n/_SCAN_BSIZE);
    int s = filterUpR(Fl,Sums,0,n,getA<bool>(Fl));
    filterDownR(Fl,Out,Sums,0,n,0,getA<ET>(In));
    delete Sums;
    return s;
  }

  template <class ET>
  void unpack(ET* In, ET* Out, bool* Fl, int n) {
    int *Sums = newA(int,1+n/_SCAN_BSIZE);
    int s = filterUpR(Fl,Sums,0,n,getA<bool>(Fl));
    unfilterDownR(Fl,In,Out,Sums,0,n,0);
    delete Sums;
  }

}

template <class T>
struct seq {
  T* A;
  int n;
  seq() {A = NULL; n=0;}
  seq(T* _A, int _n) : A(_A), n(_n) {}
  void del() {free(A);}
};
*/
#endif // _A_SEQUENCE_INCLUDED
