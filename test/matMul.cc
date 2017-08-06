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
#include <cstdlib>
#include <math.h>

#include "Job.hh"
#include "matMul.hh"
#include "machine-config.hh"
#include "parse-args.hh"
#include "utils.hh"

using namespace std;

template <class E>
void seqIterMul(denseMat<E> A, denseMat<E> B, denseMat<E> C) {
  int a_base_ptr = A.rowoff*A.rowsize + A.coloff;
  int b_base = B.rowoff*B.rowsize + B.coloff;
  int c_base_ptr = C.rowoff*C.rowsize + C.coloff;

  for (int i = 0; i < C.numrows; i++, a_base_ptr += A.rowsize, c_base_ptr += C.rowsize) {
    for (int j = 0; j < C.numcols; j++) {
      E& c = C.Values[c_base_ptr+j];
      int b_base_ptr = b_base + j;
      for (int k = 0; k < A.numcols; k++, b_base_ptr+=B.rowsize) {
	c += A.Values[a_base_ptr+k]*B.Values[b_base_ptr];
      }
    }
  }
}


template<class E>
class MatMulJob : public HR2Job {
  denseMat<E> _A, _B, _C;
  int _step;
 public :
  MatMulJob(denseMat<E> A, denseMat<E> B, denseMat<E> C, int step=0, bool del=true) 
    : HR2Job (del), _A(A), _B(B), _C(C), _step(step) {}
  
  lluint size (const int block_size) { 
    return _A.size() + _B.size() + _C.size();
  }
  lluint strand_size (const int block_size) {
    if (STRAND_SIZE_MODE==1) {
      return size(block_size);
    } else {
      if (_C.numrows<=MATMUL_BASE && _step<=1) {
	return size(block_size);
      } else {
	return STRAND_SIZE;
      }
    }
  }
  
  void function() {
    if (_step > 1) {
      join();
    } else if (_C.numrows <= MATMUL_BASE) {
      seqIterMul(_A,_B,_C);
      join();
    } else {
      Job **forked = new Job*[4];
      MatMulJob *cont = NULL;
      switch (_step) {
      case 0: 
	forked[0] = new MatMulJob(_A.topLeft(),_B.topLeft(),_C.topLeft());
	forked[1] = new MatMulJob(_A.topLeft(),_B.topRight(),_C.topRight());
	forked[2] = new MatMulJob(_A.botLeft(),_B.topLeft(),_C.botLeft());
	forked[3] = new MatMulJob(_A.botLeft(),_B.topRight(),_C.botRight());
	cont = new MatMulJob(_A,_B,_C,1);
	break;
      case 1:
	forked[0] = new MatMulJob(_A.topRight(),_B.botLeft(),_C.topLeft());
	forked[1] = new MatMulJob(_A.topRight(),_B.botRight(),_C.topRight());
	forked[2] = new MatMulJob(_A.botRight(),_B.botLeft(),_C.botLeft());
	forked[3] = new MatMulJob(_A.botRight(),_B.botRight(),_C.botRight());
	cont = new MatMulJob(_A,_B,_C,2);
	break;
      }
      fork(4, forked, cont);
    }
  }
};
 
typedef double E;

int
main (int argv, char **argc) {
  int n = (-1==get_size(argv, argc,2)) ? (1<<10) : get_size(argv, argc,2);

  Scheduler *sched=create_scheduler(argv, argc);
  
  E* space = newA(E, 3*n*n);
  stripe((char*)space,3*sizeof(E)*n*n);

  denseMat<E> A(n,n,space);
  denseMat<E> B(n,n,space+n*n);
  denseMat<E> C(n,n,space+2*n*n);
  for (int i=0; i<n*n; i++) {
    A.Values[i] = i;
    B.Values[i] = i;
    C.Values[i] = 0;
  }
  MatMulJob<E> * root = new MatMulJob<E>(A,B,C,0,false);

  startTime();
  tp_init(num_procs, map, sched, root);
  tp_sync_all();
  nextTime("Matrix multiply : ");

  //checkMatMul<E>(A,B,C);
}
