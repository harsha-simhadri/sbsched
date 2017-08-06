#include <iostream>
#include <cstdlib>
#include <math.h>

#include "mkl.h"
#include "Job.hh"
#include "matMul.hh"
#include "parse-args.hh"
#include "utils.hh"

using namespace std;

template <class E>
void mklMul(denseMat<E> A, denseMat<E> B, denseMat<E> C) {
  int a_base = A.rowoff*A.rowsize + A.coloff;
  int b_base = B.rowoff*B.rowsize + B.coloff;
  int c_base = C.rowoff*C.rowsize + C.coloff;

  cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, 
	      A.numrows, C.numcols, B.numrows, 1.0, 
	      A.Values+a_base, A.rowsize, B.Values+b_base, B.rowsize, 1.0,
	      C.Values+c_base, C.rowsize);
}

class MKLMatMulJob : public HR2Job {
  denseMat<double> _A, _B, _C;
  int _step;
 public :
  MKLMatMulJob(denseMat<double> A, denseMat<double> B, denseMat<double> C, int step=0, bool del=true) 
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
      mklMul(_A,_B,_C);
      join();
    } else {
      Job **forked = new Job*[4];
      MKLMatMulJob *cont = NULL;
      switch (_step) {
      case 0: 
	forked[0] = new MKLMatMulJob(_A.topLeft(),_B.topLeft(),_C.topLeft());
	forked[1] = new MKLMatMulJob(_A.topLeft(),_B.topRight(),_C.topRight());
	forked[2] = new MKLMatMulJob(_A.botLeft(),_B.topLeft(),_C.botLeft());
	forked[3] = new MKLMatMulJob(_A.botLeft(),_B.topRight(),_C.botRight());
	cont = new MKLMatMulJob(_A,_B,_C,1);
	break;
      case 1:
	forked[0] = new MKLMatMulJob(_A.topRight(),_B.botLeft(),_C.topLeft());
	forked[1] = new MKLMatMulJob(_A.topRight(),_B.botRight(),_C.topRight());
	forked[2] = new MKLMatMulJob(_A.botRight(),_B.botLeft(),_C.botLeft());
	forked[3] = new MKLMatMulJob(_A.botRight(),_B.botRight(),_C.botRight());
	cont = new MKLMatMulJob(_A,_B,_C,2);
	break;
      }
      fork(4, forked, cont);
    }
  }
};
 
int
main (int argv, char **argc) {
  int n = (-1==get_size(argv, argc,2)) ? (1<<10) : get_size(argv, argc,2);

  Scheduler *sched=create_scheduler(argv, argc);  
  double* space = newA(double,3*n*n);
  stripe((char*)space,3*sizeof(double)*n*n);
 

  denseMat<double> A(n,n,space);
  denseMat<double> B(n,n,space+n*n);
  denseMat<double> C(n,n,space+2*n*n);
  for (int i=0; i<n*n; i++) {
    A.Values[i] = 1;
    B.Values[i] = 1;
    C.Values[i] = 0;
  }
  MKLMatMulJob * root = new MKLMatMulJob(A,B,C,0,false);
  mkl_set_num_threads(1);  

  startTime();
  tp_init(num_procs, map, sched, root);
  tp_sync_all();
  nextTime("Matrix multiply : ");
  
  //checkMatMul<double>(A,B,C);
}
