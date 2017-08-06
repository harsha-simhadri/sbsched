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


#define MATMUL_BASE (1<<7)

template <class ETYPE>
struct denseMat {
  int numrows, numcols;
  int rowoff, coloff;
  int rowsize;
  ETYPE* Values;

  ETYPE& operator() (int row, int col) {
    return Values[(rowoff+row)*rowsize + (coloff + col)];
  }
  
  denseMat(int r, int c, ETYPE* V) : numrows(r), numcols(c), Values(V), rowoff(0), coloff(0), rowsize(c) {}
  denseMat(int r, int c, ETYPE* V, int roff, int coff, int s) : numrows(r), numcols(c), Values(V), rowoff(roff), coloff(coff), rowsize(s) {}
  
  denseMat topLeft() {
    denseMat newMat(numrows/2, numcols/2, Values, rowoff, coloff, rowsize);
    return newMat;
  }

  denseMat topRight() {
    denseMat newmat(numrows/2, numcols - numcols/2, Values, rowoff, coloff+numcols/2, rowsize);
    return newmat;
  }    

  denseMat botLeft() {
    denseMat newmat(numrows - numrows/2, numcols/2, Values, rowoff+numrows/2, coloff, rowsize);
    return newmat;
  }

  denseMat botRight() {
    denseMat newmat(numrows - numrows/2, numcols - numcols/2, Values, rowoff+numrows/2, coloff+numcols/2, rowsize);
    return newmat;
  }

  lluint size () { return numrows*numcols*sizeof (ETYPE); }  // Not really accurate, but ok for now
};


template <class E>
void checkMatMul(denseMat<E> A, denseMat<E> B, denseMat<E> C) {
  denseMat<E> D(C.numrows,C.numcols,new E[C.numrows*C.numcols]);
  for (int i = 0; i < D.numrows * D.numcols; i++) {
    D.Values[i] = 0;
  }

  for (int i = 0; i < D.numrows; i++) 
    for (int j = 0; j < D.numcols; j++) 
      for (int k = 0; k < A.numcols; k++) 
	D(i,j) += A(i,k)*B(k,j);
  
  for (int i=0; i<C.numrows * C.numcols ; i++) {
    if (C.Values[i] != D.Values[i]) {
      printf("whoops at cell %d\n", i);
      return;
    } 
  }
  printf("good\n");
}
