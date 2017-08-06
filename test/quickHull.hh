// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
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

#include <algorithm>
//#include "parallel.h"
#include "geometry.h"
#include "sequence-jobs.hh"
#include "utils.hh"

using namespace std;
//using namespace sequence;


#define GCTYPE double
typedef _point2d<GCTYPE> point2d;


template <class ET, class F>
pair<int,int> split(ET* A, int n, F lf, F rf) {
  int ll = 0, lm = 0;
  int rm = n-1, rr = n-1;
  while (1) {
    while ((lm <= rm) && !(rf(A[lm]) > 0)) {
      if (lf(A[lm]) > 0) A[ll++] = A[lm];
      lm++;
    }
    while ((rm >= lm) && !(lf(A[rm]) > 0)) {
      if (rf(A[rm]) > 0) A[rr--] = A[rm];
      rm--;
    }
    if (lm >= rm) break; 
    ET tmp = A[lm++];
    A[ll++] = A[rm--];
    A[rr--] = tmp;
  }
  int n1 = ll;
  int n2 = n-rr-1;
  return pair<int,int>(n1,n2);
}

template <class T>
struct _seq {
  T* A;
  long n;
  _seq() {A = NULL; n=0;}
_seq(T* _A, long _n) : A(_A), n(_n) {}
  //  _seq(_seq& from) {A=from.A; n=from.n;}
  void del() {free(A);}
};

struct makePair {
  pair<int,int> operator () (int i) { return pair<int,int>(i,i);}
};

struct minMaxIndex {
  point2d* P;
  minMaxIndex (point2d* _P) : P(_P) {}
  pair<int,int> operator () (pair<int,int> l, pair<int,int> r) {
    int minIndex = 
      (P[l.first].x < P[r.first].x) ? l.first :
      (P[l.first].x > P[r.first].x) ? r.first :
      (P[l.first].y < P[r.first].y) ? l.first : r.first;
    int maxIndex = (P[l.second].x > P[r.second].x) ? l.second : r.second;
    return pair<int,int>(minIndex, maxIndex);
  }
};


/*
template<class OT, class F, class G>
class Reduce : public HR2Job {
  int s,e; F f; G g;int stage=0; OT *ret; OT* ret1, ret2;
public:
  Reduce (OT *ret_, int s, int e, F f_, G g_, OT* ret1_, OT* ret2_, int stage_=0, bool del=true)
    : HR2Job(del), ret(ret_),s(s_), e(e_),f(f_),g(g_),ret1(ret1_), ret2(ret2_), stage(stage_) {}
  
  void function () {
    if (stage == 0) {
      if (e-s < _SCAN_BSIZE) {
	OT r = g(s);
	for (int j=s+1; j < e; j++) r = f(r,g(j));
	*ret = r;
	join ();
      } else {
	ret1 = newA OT; ret2 = newA OT;	m = (s+e)/2;
	binary_fork (new Reduce(ret1,s,m,f,g),
		     new Reduce(ret2,m,e,f,g),
		     new Reduce(ret,s,e,f,g,ret1,ret2,1));
      }
    } else {
      *ret = f(ret1,ret2);
      join();
    }
  }
  }*/

struct aboveLine {
  int l, r;
  point2d* P;
  aboveLine(point2d* _P, int _l, int _r) : P(_P), l(_l), r(_r) {}
  bool operator() (int i) {return triArea(P[l], P[r], P[i]) > 0.0;}
};

int serialQuickHull(int* I, point2d* P, int n, int l, int r) {
  if (n < 2) return n;
  int maxP = I[0];
  double maxArea = triArea(P[l],P[r],P[maxP]);
  for (int i=1; i < n; i++) {
    int j = I[i];
    double a = triArea(P[l],P[r],P[j]);
    if (a > maxArea) {
      maxArea = a;
      maxP = j;
    }
  }

  pair<int,int> nn = split(I, n, aboveLine(P,l,maxP), aboveLine(P,maxP,r));
  int n1 = nn.first;
  int n2 = nn.second;

  int m1, m2;
  m1 = serialQuickHull(I,      P, n1, l,   maxP);
  m2 = serialQuickHull(I+n-n2, P, n2, maxP,r);
  for (int i=0; i < m2; i++) I[i+m1+1] = I[i+n-n2];
  I[m1] = maxP;
  return m1+1+m2;
}

struct triangArea {
  int l, r;
  point2d* P;
  int* I;
  triangArea(int* _I, point2d* _P, int _l, int _r) : I(_I), P(_P), l(_l), r(_r) {}
  double operator() (int i) {return triArea(P[l], P[r], P[I[i]]);}
};
 
template <class ET, class F, class G>
class MaxIndex : public HR2Job {
  int *ret; int s, e; F f; G g;
  int stage; int* ret1; int* ret2;

public:
  MaxIndex (int *ret_, int s_, int e_, F f_, G g_,
	    int stage_=0, int* ret1_=NULL, int* ret2_=NULL, bool del=true)
    : HR2Job(del), ret(ret_), s(s_), e(e_), f(f_), g(g_), 
      ret1(ret1_), ret2(ret2_),stage(stage_) {}

  //MaxIndex (int *ret_, ET* A, int n, F f_)
  //  : HR2Job(del), ret(ret_), s(0), e(n), f(f_), g(getA<ET,int>(A)), stage(stage_) {}

  lluint size (const int block_size) {return 0;}
  lluint strand_size (const int block_size) {return 0;}

  void function() {
    if (stage == 0) {
      if (e-s < _SCAN_BSIZE) {
	ET r = g(s);
	int k = 0;
	for (int j=s+1; j < e; j++) {
	  ET v = g(j);
	  if (f(v,r)) { r = v; k = j;}
	}
	*ret = k;
	join();
      } else {
	int *ret1 = newA(int,1); int *ret2 = newA (int,1);
	int m=(s+e)/2;
	binary_fork (new MaxIndex<ET,F,G>(ret1,s,m,f,g),
		     new MaxIndex<ET,F,G>(ret2,m,e,f,g),
		     new MaxIndex<ET,F,G>(ret,s,e,f,g,1,ret1,ret2));
      }
    } else {
      *ret = f(g(*ret1),g(*ret2)) ? *ret1 : *ret2;
      join();
    }
  }
};

class QuickHull : public HR2Job {
  int* I; int* Itmp; point2d* P; int n; int l; int r; int depth;
  int *maxIdx; int *n1,*n2,*m1,*m2;
  int *ret; int stage;

public:
  QuickHull (int *ret_, int* I_, int* Itmp_, point2d* P_, int n_, int l_, int r_, int depth_,
	     int *maxIdx_=NULL, int* n1_=NULL, int* n2_=NULL, int* m1_=NULL, int* m2_=NULL, int stage_=0, bool del=true) 
    : HR2Job(del), ret(ret_), 
      I(I_), Itmp(Itmp_), P(P_), n(n_), l(l_), r(r_), depth(depth_),
      maxIdx(maxIdx_), n1(n1_), n2(n2_), stage(stage_) {}

  QuickHull (QuickHull *from, int stage_=-1, bool del=true)
    : HR2Job (del), ret(from->ret),
      I(from->I), Itmp(from->Itmp), P(from->P), n(from->n), l(from->l), r(from->r), depth(from->depth),
      maxIdx(from->maxIdx), n1(from->n1), n2(from->n2), m1(from->m1), m2(from->m2), stage(1+from->stage) {}

  lluint size (const int block_size) {return 0;}
  lluint strand_size (const int block_size) {return 0;}

  void function() {
    if (stage == 0) {
      if (n<2) {
	*ret = serialQuickHull(I, P, n, l, r);
	join ();
      } else {
	maxIdx = newA(int,1);
	unary_fork(new MaxIndex<double,greater<double>,triangArea >(maxIdx,0,n,greater<double>(),triangArea(I,P,l,r)),
		   new QuickHull(this));
      }
    } else if (stage == 1) {
      int maxP = I[*maxIdx];
      int *n1 = newA(int,1); int *n2=newA(int,1); int *m1 = newA(int,1); int *m2 = newA(int,1);
      unary_fork(new Filter<int,aboveLine,getA<int> > (n1,Itmp,0,n,aboveLine(P,l,maxP),getA<int>(I)),
		 new QuickHull(this));
    } else if (stage == 2) {
      int maxP = I[*maxIdx];
      unary_fork(new Filter<int,aboveLine,getA<int> > (n2,Itmp+*n1,0,n,aboveLine(P,maxP,r),getA<int>(I)),
		 new QuickHull(this));
    } else if (stage == 3) {
      int maxP = I[*maxIdx];
      binary_fork (new QuickHull(m1,Itmp,I,P,*n1,l,maxP,depth-1),
		   new QuickHull(m2,Itmp+*n1,I+*n1,P,*n2,maxP,r,depth-1),
		   new QuickHull(this));
    } else if (stage == 4) {
      unary_fork (new Map<int,int,Id<int> > (I, Itmp, *m1, Id<int>()),
		  new QuickHull(this));
    } else if (stage == 4) {
      int maxP = I[*maxIdx];
      I[*m1] = maxP;
      unary_fork (new Map<int,int,Id<int> > (I+*m1+1, Itmp+*n1, *m2, Id<int>()),
		  new QuickHull(this));
    } else {
      *ret = *m1+1+*m2;
      join();
    }
  }
};
  
class Hull : public HR2Job {
  point2d* P; int n;
  int l,r; bool *fTop;bool* fBot;int *I;int *Itmp;pair<int,int> *minMax;
  int *n1; int *n2; int *m1; int *m2;
  int stage; _seq<int> *ret;

public:

  Hull (_seq<int> *ret_, point2d* P_, int n_,
	int stage_=0, bool del=true)
    : HR2Job(del), ret(ret_), P(P_), n(n_), stage(stage_) {}

  Hull (Hull *from, bool del=true)
    : HR2Job(del), P(from->P), n(from->n), l(from->l), r(from->r),
      fTop(from->fTop), fBot(from->fBot), I(from->I), Itmp(from->Itmp),
      stage(1+(from->stage)), minMax(from->minMax), n1(from->n1), n2(from->n2),
      m1(from->m1), m2(from->m2), ret(from->ret) {}

  lluint size (const int block_size) {return 0;}
  lluint strand_size (const int block_size) {return 0;}  

  void function () {
    printf("Stage: %d\n", stage);
    if (stage == 0) {
      minMax = (pair<int,int>*) malloc (sizeof(pair<int,int>));
      unary_fork (new Reduce<pair<int,int>,minMaxIndex,makePair >(minMax,(int)0,n,minMaxIndex(P),makePair()),
		  new Hull(this));
    } else if (stage == 1) {
      int l = minMax->first;	int r = minMax->second;
      fTop = newA(bool,n); fBot = newA(bool,n);
      int* I = newA(int, n); int* Itmp = newA(int, n);
      unary_fork (new MapInt<int,Id<int> > (Itmp, n, Id<int>()),
		  new Hull(this));
    } else if (stage == 2) {
      int l = minMax->first;	int r = minMax->second;      
      printf("Left: %d, Right: %d\n", l,r);
      unary_fork (new MapInt<bool,aboveLine> (fTop,n,aboveLine(P,l,r)),
		  new Hull(this));
    } else if (stage == 3) {
      int l = minMax->first;	int r = minMax->second;
      unary_fork (new MapInt<bool,aboveLine> (fBot,n,aboveLine(P,r,l)),
		  new Hull(this));
    } else if (stage == 4) {
      n1 = new int; n2 = new int; m1 = new int; m2 = new int;
      //unary_fork (new Pack<int> (n1,Itmp,I,fTop,n),
      unary_fork (new Filter<int,aboveLine,getA<int> >(n1,I,0,n,aboveLine(P,l,r), getA<int> (Itmp)),
		  new Hull(this));
    } else if (stage == 5) {
      unary_fork (new Pack<int> (n2,Itmp,I+*n1,fBot,n),
		  new Hull(this));
    } else if (stage == 6) {
      free (fTop); free (fBot);
      binary_fork (new QuickHull(m1, I, Itmp, P, *n1, l, r, 5),
		   new QuickHull(m2,I+*n1,  Itmp+*n1, P, *n2, r, l, 5),
		   new Hull(this));
    } else if (stage == 7) {
      unary_fork (new Map<int, int, Id<int> >(I,Itmp+1,*m1,Id<int>()),
		  new Hull(this));
    } else if (stage == 8) {
      unary_fork (new Map<int, int, Id<int> >(I+*n1,Itmp+1+*m1,*m2,Id<int>()),
		  new Hull(this));
    } else {
      Itmp[0]=l;
      Itmp[*m1+1]=r;
      ret->A=Itmp; ret->n=*m1+*m2+2;
      free(I);
      join();
    }
  }
};    
