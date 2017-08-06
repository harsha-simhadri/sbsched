#include <stdio.h>
#include <stdlib.h>

#define E_TYPE int
E_TYPE *A;
E_TYPE *B;

void
initloop (long l) {
  int i;
  A = (E_TYPE *) malloc (sizeof(E_TYPE)*l + 1000);
  B = (E_TYPE *) malloc (sizeof(E_TYPE)*l + 1000);
  for (i=0; i<l; ++i) {
    A[i]=i; B[i]=2*i;
  }
}

void
copyloop (long l) {
  int i;int sum=0;
  for (i=0; i<l; ++i)   {
    A[i] = B[i]; sum += A[i];
  }
  printf ("Copy loop sum: %d\n", sum);
}

int main () {

  int LEN=(1<<27);
  initloop(LEN);
  //copyloop(LEN);
}
