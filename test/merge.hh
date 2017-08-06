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


#include <algorithm>

#define _MERGE_BSIZE (1<<13)

template <class ET, class F> 
int binSearch(ET* S, int n, ET v, F f) {
  if (n == 0) return 0;
  else { 
    int mid = n/2;
    if (f(v,S[mid])) return binSearch(S, mid, v, f);
    else return mid + 1 + binSearch(S+mid+1, (n-mid)-1, v, f);
  }
}

template <class ET, class F> 
class Merge : public SizedJob {

  ET* S1; int l1; ET* S2; int l2; ET* R; F f;
  int stage;
  
public:
  
  Merge (ET* S1_, int l1_, ET* S2_, int l2_, ET* R_, F f_,
	 bool del=true)
    : SizedJob (del),
      S1 (S1_), l1(l1_), S2(S2_), l2(l2_), R(R_), f(f_),
      stage(0)
    {}

  Merge (int l1_, int l2_,
	 bool del=true)
    : SizedJob (del),
      l1(l1_), l2(l2_), 
      stage(1)
    {}

  lluint size (const int block_size) {
    return block_size * (int)ceil ( (l1+l2)/((double)block_size));
  }
  
  void function () {
    if (stage == 0) {
      int lr = l1 + l2;
      if (lr > _MERGE_BSIZE) {
	if (l2>l1) {
	  unary_fork (new Merge<ET,F>(S2,l2,S1,l1,R,f),
		      new Merge<ET,F>(l2,l1));
	} else {
	  int m1 = l1/2;
	  int m2 = binSearch(S2,l2,S1[m1],f);
	  binary_fork (new Merge<ET,F>(S1,m1,S2,m2,R,f),
		       new Merge<ET,F>(S1+m1,l1-m1,S2+m2,l2-m2,R+m1+m2,f),
		       new Merge<ET,F>(l1,l2));
	}
      } else {
	ET* pR = R; 
	ET* pS1 = S1; 
	ET* pS2 = S2;
	ET* eS1 = S1+l1; 
	ET* eS2 = S2+l2;
	while (true) {
	  *pR++ = f(*pS2,*pS1) ? *pS2++ : *pS1++;
	  if (pS1==eS1) {std::copy(pS2,eS2,pR); break;}
	  if (pS2==eS2) {std::copy(pS1,eS1,pR); break;}
	}
	join();
      }
    } else {
      join();
    }
  }
};
