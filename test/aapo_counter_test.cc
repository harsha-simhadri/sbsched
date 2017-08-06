
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


#include <inttypes.h>           /* for PRIu64 definition */
#include <stdint.h>             /* for uint64_t and PRIu64 */
#include <stdlib.h>
#include <cmath>
#include "libperf.h"
#include <iostream>

#define CACHELINE 64  // bytes

typedef void(*testFunction)(int _n);

void flush_cache (int len) {
  uint64_t *flush = new uint64_t[len]; uint64_t sum;
  for (int i=0; i<len; ++i)
    sum += flush[i];
  delete flush;
}

uint64_t* A;
uint64_t* B;
struct perf_data* pd; 
  


void initloop(int n) {
  for (register  int i=0; i<n; ++i) {
    A[i] = i;
    B[i] = 2*i;
  }
}

void copyloop(int n) {
  for (register int i=0; i<n; ++i)
    B[i] = 1+A[i];
}

void copyloopptr(int n) {
  register uint64_t * Ap = A;
  register uint64_t * Bp = B;
  
  for (register int i=0; i<n; ++i)
    *Bp++ = 1+*Ap++;
}

void transformloop(int n) {
  for (register int i=0; i<n; ++i)
    A[i] = 1+A[i];
}

void setA(int n) {
  for (register int i=0; i<n; ++i)
    A[i] =  i;
}
void nothing(int n) {
    register uint64_t x = 0;

    for (register int i=0; i<n; ++i) { x+=A[0];}
}

void almostnothing(int n) {
    register uint64_t x = 0;

    for (register int i=0; i<n; ++i) { x+=A[0];}
    if(x == 1) std::cout << "One!" ; // Just to make sure compiler does not optimize the loop away
}

void sumloop(int n) {
    register uint64_t x = 0;
    for (register  int i=0; i<n; ++i)
        x += A[i];
    if(x == 0) std::cout << "Zero!" ; // Just to make sure compiler does not optimize the loop away
}

void sumloopptr(int n) {
    register uint64_t x = 0;
    register uint64_t * Ap = A;
    for (register  int i=0; i<n; ++i)
        x += *Ap++;
    if(x == 0) std::cout << "Zero!" ; // Just to make sure compiler does not optimize the loop away
}


void sumloopptr_unroll8(int n) {
    register uint64_t x = 0;
    register uint64_t * Ap = A;
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


void jumpysumloop(int n) {
    register uint64_t x = 0;
    for (register int i=0; i<n; ++i)
        x += A[(i*33397)%n];
    if(x == 0) std::cout << "Zero!" ; // Just to make sure compiler does not optimize the loop away
}



void runtest(std::string name, testFunction f, int n) {
  libperf_enablecounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS);
  libperf_enablecounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES);
  
  uint64_t LLD_LL_init = libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS);
  uint64_t LLD_LM_init = libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES);
  
  
    f(n);
    
  uint64_t LLD_LL = libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS) - LLD_LL_init;
  uint64_t LLD_LM = libperf_readcounter (pd,LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES) - LLD_LM_init;

  libperf_disablecounter(pd, LIBPERF_COUNT_HW_CACHE_LL_LOADS);                 /* disable HW counter */
  libperf_disablecounter(pd, LIBPERF_COUNT_HW_CACHE_LL_LOADS_MISSES);                 /* disable HW counter */
  
  std::cout << n << " " << name << ", LLD_LL: " << LLD_LL << std::endl;
  std::cout << n << " " << name << ", LLD_LM: " << LLD_LM << std::endl;
  if (LLD_LL>0)
    std::cout << "Misses/loads: " << (LLD_LM*1.0/LLD_LL) << std::endl;
  
}
  
int
main (int argv, char **argc) {
  int LEN = 100000000;
  if (argv>=2)
    LEN = atoi (argc[1]);

  A = new uint64_t[LEN];
   B = new uint64_t[LEN];
  std::cout << "Length is " << LEN << std::endl;

  
  flush_cache(LEN);

  pd = libperf_initialize (-1,-1);
               /* disable HW counter */
               
  runtest("init", initloop, LEN);               
  runtest("copyloop", copyloop, LEN);          
  runtest("copyloopptr", copyloopptr, LEN);               

/*
  runtest("sumloop", sumloop, LEN);
  runtest("sumloopptr", sumloopptr, LEN);
  runtest("sumloopptr_unroll8", sumloopptr_unroll8, LEN);

  runtest("transformloop", transformloop, LEN);               
  runtest("setA", setA, LEN);               

  runtest("jumpysumloop", jumpysumloop, LEN);               
  runtest("nothing", nothing, LEN);               
  runtest("almostnothing", almostnothing, LEN);               
*/
  libperf_finalize (pd,NULL);

}

