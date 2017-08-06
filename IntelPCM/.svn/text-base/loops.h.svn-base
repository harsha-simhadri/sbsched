#include <inttypes.h>           /* for PRIu64 definition */
#include <stdint.h>             /* for uint64_t and PRIu64 */
#include <stdlib.h>
#include <cmath>
#include <iostream>

#define CACHELINE 64  // bytes

typedef uint64_t E;

typedef void (*testFunction)(int,int);

void flush_cache (int len) {
  E *flush = new E[len]; E sum;
  for (int i=0; i<len; ++i)
    flush[i]=3*i;
  for (int j=1; j<3; ++j)
    for (int i=0; i<len; ++i)
      sum += flush[i];
  delete flush;
  if (sum==0) {std::cout<<"Something\n"<<std::endl;}
}

E* A;
E* B;
struct perf_data* pd; 

void initloop(int n) {
  A = new E[n];
  B = new E[n];
  for (register  int i=0; i<n; ++i) {
    A[i] = i;
    B[i] = 2*i;
  }
}

void copyloop(int n, int repeats=1) {
  for (register int j=0; j<repeats; ++j)
    for (register int i=0; i<n; ++i)
      B[i] = 1+A[i];
}

void copyloopptr(int n, int repeats=1) {
  register E * Ap = A;
  register E * Bp = B;
  
  for (register int j=0; j<repeats; ++j)
    for (register int i=0; i<n; ++i)
      *Bp++ = 1+*Ap++;
}

void transformloop(int n, int repeats=1) {
  for (register int j=0; j<repeats; ++j) 
    for (register int i=0; i<n; ++i)
      A[i] = 1+A[i];
  if (A[n/repeats]==0) std::cout<<"Something"<<std::endl;
}

void setA(int n) {
  for (register int i=0; i<n; ++i)
    A[i] =  i;
}
void nothing(int n) {
    register E x = 0;

    for (register int i=0; i<n; ++i) { x+=A[0];}
}

void almostnothing(int n) {
    register E x = 0;

    for (register int i=0; i<n; ++i) { x+=A[0];}
    if(x == 1) std::cout << "One!" ; // Just to make sure compiler does not optimize the loop away
}

void sumloop(int n, int repeats=1) {
    register E x = 0;
    for (register int j=0; j<repeats; ++j) 
      for (register  int i=0; i<n; ++i)
	x += A[i];
    if(x == 0) std::cout << "Zero!" ; // Just to make sure compiler does not optimize the loop away
}

void sumloopptr(int n, int repeats=1) {
    register E x = 0;
    register E * Ap = A;
    for (register int j=0; j<repeats; ++j) 
      for (register  int i=0; i<n; ++i)
        x += *Ap++;
    if(x == 0) std::cout << "Zero!" ; // Just to make sure compiler does not optimize the loop away
}


void sumloopptr_unroll8(int n, int repeats=1) {
    register E x = 0;
    register E * Ap = A;
    for (register int j=0; j<repeats; ++j)
      for (register  int i=0; i<n; i+=8) {
        x += *Ap++;
        x += *Ap++;
        x += *Ap++;
        x += *Ap++;
        x += *Ap++;
        x += *Ap++;
        x += *Ap++;
        x += *Ap++;
    }
    if(x == 0) std::cout << "Zero!" ; // Just to make sure compiler does not optimize the loop away
}


void jumpysumloop(int n, int repeats=1) {
    register E x = 0;
    for (register int j=0; j<repeats; ++j) 
      for (register int i=0; i<n; ++i)
        x += A[(i*33397)%n];
    if(x == 0) std::cout << "Zero!" ; // Just to make sure compiler does not optimize the loop away
}

